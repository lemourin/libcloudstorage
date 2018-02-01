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

using namespace std::placeholders;

namespace cloudstorage {

Response::Response(IHttpRequest::Response r) : http_(r) {}

int Response::http_code() const { return http_.http_code_; }

const IHttpRequest::HeaderParameters& Response::headers() const {
  return http_.headers_;
}

std::stringstream& Response::output() {
  return static_cast<std::stringstream&>(*http_.output_stream_.get());
}

template <class T>
Request<T>::Wrapper::Wrapper(typename Request<T>::Pointer r) : request_(r) {}

template <class T>
Request<T>::Wrapper::~Wrapper() {
  cancel();
}

template <class T>
void Request<T>::Wrapper::finish() {
  request_->finish();
}

template <class T>
void Request<T>::Wrapper::cancel() {
  request_->cancel();
}

template <class T>
T Request<T>::Wrapper::result() {
  return request_->result();
}

template <class T>
Request<T>::Request(std::shared_ptr<CloudProvider> provider, Callback callback,
                    Resolver resolver)
    : future_(value_.get_future()),
      resolver_(resolver),
      callback_(callback),
      provider_(provider),
      is_cancelled_(false) {}

template <class T>
Request<T>::~Request() {
  cancel();
}

template <class T>
void Request<T>::finish() {
  std::shared_future<T> future = future_;
  if (future.valid()) future.wait();
  {
    std::unique_lock<std::mutex> lock(subrequest_mutex_);
    for (size_t i = 0; i < subrequests_.size(); i++) {
      auto r = subrequests_[i];
      lock.unlock();
      r->finish();
      lock.lock();
    }
  }
  {
    std::unique_lock<std::mutex> lock(provider_mutex_);
    provider_ = nullptr;
  }
}

template <class T>
void Request<T>::cancel() {
  is_cancelled_ = true;
  {
    std::unique_lock<std::mutex> lock(provider_mutex_);
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
        auto auth = util::exchange(p->current_authorization_, nullptr);
        lock.unlock();
        auth->cancel();
      }
    }
  }
  {
    std::unique_lock<std::mutex> lock(subrequest_mutex_);
    for (size_t i = 0; i < subrequests_.size(); i++) {
      auto r = subrequests_[i];
      lock.unlock();
      r->cancel();
      lock.lock();
    }
  }
  finish();
}

template <class T>
T Request<T>::result() {
  finish();
  return future_.get();
}

template <typename T>
typename Request<T>::Wrapper::Pointer Request<T>::run() {
  if (!resolver_) throw std::runtime_error("resolver not set");
  util::exchange(resolver_, nullptr)(this->shared_from_this());
  return util::make_unique<Wrapper>(this->shared_from_this());
}

template <class T>
void Request<T>::done(const T& t) {
  if (!callback_) throw std::runtime_error("no callback");
  util::exchange(callback_, nullptr)(t);
  value_.set_value(t);
}

template <class T>
std::unique_ptr<HttpCallback> Request<T>::http_callback(
    ProgressFunction progress_download, ProgressFunction progress_upload) {
  return util::make_unique<HttpCallback>(
      [=] { return is_cancelled(); },
      std::bind(&CloudProvider::isSuccess, provider_.get(), _1, _2),
      progress_download, progress_upload);
}

template <class T>
void Request<T>::reauthorize(AuthorizeCompleted c) {
  auto p = provider();
  std::unique_lock<std::mutex> lock(p->current_authorization_mutex_);
  if (is_cancelled()) {
    lock.unlock();
    return c(Error{IHttpRequest::Aborted, ""});
  }
  p->auth_callbacks_[this].push_back(c);
  if (!p->current_authorization_) {
    {
      auto r = p->authorizeAsync();
      p->current_authorization_ = r;
      lock.unlock();
      r->run();
    }
    lock.lock();
  }
  auto r = p->current_authorization_;
  lock.unlock();
  if (r) subrequest(r);
}

template <class T>
void Request<T>::authorize(IHttpRequest::Pointer r) {
  if (r) provider()->authorizeRequest(*r);
}

template <class T>
bool Request<T>::reauthorize(int code,
                             const IHttpRequest::HeaderParameters& h) {
  return provider()->reauthorize(code, h);
}

template <class T>
void Request<T>::request(RequestFactory factory, RequestCompleted complete) {
  this->send(factory, complete,
             [] { return std::make_shared<std::stringstream>(); },
             std::make_shared<std::stringstream>(), nullptr, nullptr, true);
}

template <class T>
void Request<T>::send(RequestFactory factory, RequestCompleted complete) {
  this->send(factory, complete,
             [] { return std::make_shared<std::stringstream>(); },
             std::make_shared<std::stringstream>(), nullptr, nullptr, false);
}

template <class T>
void Request<T>::send(RequestFactory factory, RequestCompleted complete,
                      InputFactory input_factory,
                      std::shared_ptr<std::ostream> output,
                      ProgressFunction download, ProgressFunction upload,
                      bool authorized) {
  auto request = this->shared_from_this();
  auto input = input_factory();
  auto error_stream = std::make_shared<std::stringstream>();
  auto r = factory(input);
  if (authorized) authorize(r);
  send(r.get(),
       [=](IHttpRequest::Response response) {
         if (provider()->isSuccess(response.http_code_, response.headers_))
           return complete(Response(response));
         if (authorized &&
             this->reauthorize(response.http_code_, response.headers_)) {
           this->reauthorize([=](EitherError<void> e) {
             if (e.left()) {
               if (e.left()->code_ != IHttpRequest::Aborted &&
                   e.left()->code_ > 0)
                 return complete(
                     Error{IHttpRequest::Unauthorized, e.left()->description_});
               else
                 return complete(
                     Error{response.http_code_, error_stream->str()});
             }
             auto input = input_factory();
             auto error_stream = std::make_shared<std::stringstream>();
             auto r = factory(input);
             if (authorized) authorize(r);
             this->send(
                 r.get(),
                 [=](IHttpRequest::Response response) {
                   (void)request;
                   if (provider()->isSuccess(response.http_code_,
                                             response.headers_))
                     complete(Response(response));
                   else
                     complete(Error{response.http_code_, error_stream->str()});
                 },
                 input, output, error_stream, download, upload);
           });
         } else {
           complete(Error{response.http_code_, error_stream->str()});
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
                  http_callback(download, upload));
  else
    complete({IHttpRequest::Aborted, {}, output, error});
}

template <class T>
std::shared_ptr<CloudProvider> Request<T>::provider() const {
  return provider_;
}

template <class T>
bool Request<T>::is_cancelled() const {
  return is_cancelled_;
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

template class Request<EitherError<PageData>>;
template class Request<EitherError<Token>>;
template class Request<EitherError<std::vector<char>>>;
template class Request<EitherError<std::string>>;
template class Request<EitherError<IItem>>;
template class Request<EitherError<std::vector<IItem::Pointer>>>;
template class Request<EitherError<void>>;
template class Request<EitherError<GeneralData>>;

}  // namespace cloudstorage
