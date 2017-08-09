/*****************************************************************************
 * Request.cpp : Request implementation
 *
 *****************************************************************************
 * Copyright (C) 2016-2016 VideoLAN
 *
 * Authors: Paweł Wegner <pawel.wegner95@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "Request.h"

#include "CloudProvider/CloudProvider.h"
#include "HttpCallback.h"
#include "Utility/Utility.h"

#include <algorithm>
#include <cctype>

namespace cloudstorage {

template <class T>
Request<T>::Request(std::shared_ptr<CloudProvider> provider)
    : future_(value_.get_future()),
      provider_shared_(provider),
      is_cancelled_(false) {}

template <class T>
Request<T>::Request(std::weak_ptr<CloudProvider> provider)
    : future_(value_.get_future()),
      provider_weak_(provider),
      is_cancelled_(false) {}

template <class T>
Request<T>::~Request() {
  cancel();
}

template <class T>
void Request<T>::finish() {
  std::shared_future<T> future = future_;
  if (future.valid()) future.wait();
}

template <class T>
void Request<T>::cancel() {
  if (is_cancelled()) return;
  set_cancelled(true);
  {
    std::lock_guard<std::mutex> lock(subrequest_mutex_);
    for (auto r : subrequests_) r->cancel();
  }
  auto p = provider();
  if (p) {
    std::unique_lock<std::mutex> lock(p->current_authorization_mutex_);
    auto it = p->auth_callbacks_.find(this);
    if (it != std::end(p->auth_callbacks_)) {
      while (!it->second.empty()) {
        {
          auto c = it->second.back();
          it->second.pop_back();
          lock.unlock();
          c(Error{IHttpRequest::Aborted, ""});
        }
        lock.lock();
      }
      p->auth_callbacks_.erase(it);
    }
    if (p->auth_callbacks_.empty() && p->current_authorization_) {
      auto auth = std::move(p->current_authorization_);
      lock.unlock();
      auth->cancel();
    }
  }
  finish();
}

template <class T>
T Request<T>::result() {
  std::shared_future<T> future = future_;
  return future.get();
}

template <class T>
void Request<T>::set(Resolver r) {
  resolver_ = r;
}

template <typename T>
typename Request<T>::Ptr Request<T>::run() {
  resolver_(this->shared_from_this());
  return this->shared_from_this();
}

template <class T>
void Request<T>::done(const T& t) {
  value_.set_value(t);
}

template <class T>
std::unique_ptr<HttpCallback> Request<T>::httpCallback(
    std::function<void(uint32_t, uint32_t)> progress_download,
    std::function<void(uint32_t, uint32_t)> progress_upload) {
  return util::make_unique<HttpCallback>([=] { return is_cancelled(); },
                                         progress_download, progress_upload);
}

template <class T>
void Request<T>::reauthorize(AuthorizeCompleted c) {
  auto p = provider();
  std::unique_lock<std::mutex> lock(p->current_authorization_mutex_);
  if (!p->current_authorization_)
    p->current_authorization_ = p->authorizeAsync();
  p->auth_callbacks_[this].push_back(c);
}

template <class T>
void Request<T>::sendRequest(RequestFactory factory, RequestCompleted complete,
                             std::shared_ptr<std::ostream> output,
                             ProgressFunction download,
                             ProgressFunction upload) {
  auto p = provider();
  auto request = this->shared_from_this();
  if (!p) return;
  auto input = std::make_shared<std::stringstream>(),
       error_stream = std::make_shared<std::stringstream>();
  auto r = factory(input);
  if (r) p->authorizeRequest(*r);
  send(r.get(),
       [=](int code, util::Output, util::Output) {
         if (IHttpRequest::isSuccess(code)) return complete(output);
         if (p->reauthorize(code)) {
           this->reauthorize([=](EitherError<void> e) {
             if (e.left()) {
               if (e.left()->code_ != IHttpRequest::Aborted)
                 return complete(e.left());
               else
                 return complete(Error{code, error_stream->str()});
             }
             auto input = std::make_shared<std::stringstream>(),
                  error_stream = std::make_shared<std::stringstream>();
             auto r = factory(input);
             if (r) p->authorizeRequest(*r);
             this->send(r.get(),
                        [=](int code, util::Output output, util::Output) {
                          (void)request;
                          if (IHttpRequest::isSuccess(code))
                            complete(output);
                          else
                            complete(Error{code, error_stream->str()});
                        },
                        input, output, error_stream, download, upload);
           });
         } else {
           complete(Error{code, error_stream->str()});
         }
       },
       input, output, error_stream, download, upload);
}

template <class T>
void Request<T>::send(IHttpRequest* request,
                      IHttpRequest::CompleteCallback complete,
                      std::shared_ptr<std::istream> input,
                      std::shared_ptr<std::ostream> output,
                      std::shared_ptr<std::ostream> error,
                      ProgressFunction download, ProgressFunction upload) {
  if (request)
    request->send(complete, input, output, error,
                  httpCallback(download, upload));
  else
    complete(IHttpRequest::Aborted, output, error);
}

template <class T>
std::shared_ptr<CloudProvider> Request<T>::provider() const {
  if (provider_shared_)
    return provider_shared_;
  else
    return provider_weak_.lock();
}

template <class T>
bool Request<T>::is_cancelled() {
  return is_cancelled_;
}

template <class T>
void Request<T>::set_cancelled(bool e) {
  std::lock_guard<std::mutex> lock(cancelled_mutex_);
  is_cancelled_ = e;
}

template <class T>
std::unique_lock<std::mutex> Request<T>::cancelled_lock() {
  return std::unique_lock<std::mutex>(cancelled_mutex_);
}

template <class T>
void Request<T>::subrequest(std::shared_ptr<IGenericRequest> request) {
  if (is_cancelled())
    request->cancel();
  else {
    std::lock_guard<std::mutex> lock(subrequest_mutex_);
    subrequests_.push_back(request);
  }
}

template class Request<EitherError<std::vector<char>>>;
template class Request<EitherError<std::string>>;
template class Request<EitherError<IItem>>;
template class Request<EitherError<std::vector<IItem::Pointer>>>;
template class Request<EitherError<void>>;

}  // namespace cloudstorage
