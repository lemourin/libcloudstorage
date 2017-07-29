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

namespace {

bool isPrintable(const std::string& str) {
  for (char c : str)
    if (!isprint(c) && !isspace(c)) return false;
  return true;
}

}  // namespace

template <class T>
Request<T>::Request(std::shared_ptr<CloudProvider> provider)
    : provider_shared_(provider), is_cancelled_(false) {}

template <class T>
Request<T>::Request(std::weak_ptr<CloudProvider> provider)
    : provider_weak_(provider), is_cancelled_(false) {}

template <class T>
Request<T>::~Request() {
  cancel();
}

template <class T>
void Request<T>::set_resolver(Resolver resolver) {
  function_ = std::async(std::launch::async, std::bind(resolver, this));
}

template <class T>
void Request<T>::set_error_callback(ErrorCallback f) {
  error_callback_ = f;
}

template <class T>
void Request<T>::set_cancel_callback(CancelCallback f) {
  cancel_callback_ = f;
}

template <class T>
void Request<T>::finish() {
  std::shared_future<T> future = function_;
  if (future.valid()) future.wait();
}

template <class T>
void Request<T>::cancel() {
  if (is_cancelled()) return;
  set_cancelled(true);
  {
    std::lock_guard<std::mutex> lock(semaphore_list_mutex_);
    for (Semaphore* semaphore : semaphore_list_) semaphore->notify();
  }
  if (cancel_callback_) cancel_callback_();
  auto p = provider();
  if (p) p->authorized_condition().notify_all();
  finish();
}

template <class T>
T Request<T>::result() {
  std::shared_future<T> future = function_;
  return future.get();
}

template <class T>
std::unique_ptr<HttpCallback> Request<T>::httpCallback(
    std::function<void(uint32_t, uint32_t)> progress_download,
    std::function<void(uint32_t, uint32_t)> progress_upload) {
  return util::make_unique<HttpCallback>(is_cancelled_, progress_download,
                                         progress_upload);
}

template <class T>
bool Request<T>::reauthorize() {
  auto p = provider();
  if (!p || is_cancelled()) return false;
  std::unique_lock<std::mutex> current_authorization(
      p->current_authorization_mutex());
  if (!p->current_authorization()) {
    p->set_authorization_status(CloudProvider::AuthorizationStatus::InProgress);
    p->set_current_authorization(p->authorizeAsync());
  }
  p->set_authorization_request_count(p->authorization_request_count() + 1);
  p->authorized_condition().wait(current_authorization, [this, p] {
    return p->authorization_status() !=
               CloudProvider::AuthorizationStatus::InProgress ||
           is_cancelled();
  });
  p->set_authorization_request_count(p->authorization_request_count() - 1);
  bool ret =
      p->authorization_status() == CloudProvider::AuthorizationStatus::Success;
  if (p->authorization_request_count() == 0) {
    p->set_current_authorization(nullptr);
    p->set_authorization_status(CloudProvider::AuthorizationStatus::None);
  }
  return ret;
}

template <class T>
void Request<T>::error(int code, const std::string& desc) {
  if (error_callback_) error_callback_(this, code, desc);
}

template <class T>
std::string Request<T>::error_string(int code, const std::string& desc) const {
  std::stringstream stream;
  if (code < 0) {
    code *= -1;
    stream << "HTTP library error " << code;
    auto p = provider();
    if (p) stream << ": " << p->http()->error(code);
  } else {
    stream << "HTTP code " << code;
    if (isPrintable(desc)) stream << ": " << desc;
  }
  return stream.str();
}

template <class T>
int Request<T>::sendRequest(
    std::function<IHttpRequest::Pointer(std::ostream&)> factory,
    std::ostream& output, Error* error, ProgressFunction download,
    ProgressFunction upload) {
  auto p = provider();
  if (!p) return IHttpRequest::Unknown;
  std::stringstream input, error_stream;
  auto request = factory(input);
  if (request) p->authorizeRequest(*request);
  int code =
      send(request.get(), input, output, &error_stream, download, upload);
  if (IHttpRequest::isSuccess(code)) return code;
  if (p->reauthorize(code)) {
    if (!reauthorize()) {
      if (!is_cancelled()) this->error(code, error_stream.str());
    } else {
      std::stringstream input, error_stream;
      request = factory(input);
      if (request) p->authorizeRequest(*request);
      code =
          send(request.get(), input, output, &error_stream, download, upload);
      if (!is_cancelled() && !IHttpRequest::isSuccess(code))
        this->error(code, error_stream.str());
    }
  } else {
    if (!is_cancelled() && code != IHttpRequest::Aborted)
      this->error(code, error_stream.str());
  }
  if (error) *error = {code, error_stream.str()};
  return code;
}

template <class T>
int Request<T>::send(IHttpRequest* request, std::istream& input,
                     std::ostream& output, std::ostream* error,
                     ProgressFunction download, ProgressFunction upload) {
  if (!request) {
    this->error(IHttpRequest::Aborted, "Not available.");
    return IHttpRequest::Aborted;
  }
  return request->send(input, output, error, httpCallback(download, upload));
}

template <class T>
std::shared_ptr<CloudProvider> Request<T>::provider() const {
  if (provider_shared_)
    return provider_shared_;
  else
    return provider_weak_.lock();
}

template <class T>
Request<T>::Semaphore::Semaphore(Request* request) : request_(request) {
  std::lock_guard<std::mutex> lock(request_->semaphore_list_mutex_);
  request_->semaphore_list_.push_back(this);
  if (request_->is_cancelled()) notify();
}

template <class T>
Request<T>::Semaphore::~Semaphore() {
  std::lock_guard<std::mutex> lock(request_->semaphore_list_mutex_);
  const auto& list = request_->semaphore_list_;
  request_->semaphore_list_.erase(std::find(list.begin(), list.end(), this));
}

template class Request<EitherError<std::vector<char>>>;
template class Request<EitherError<std::string>>;
template class Request<EitherError<IItem>>;
template class Request<EitherError<std::vector<IItem::Pointer>>>;
template class Request<EitherError<void>>;

}  // namespace cloudstorage
