/*****************************************************************************
 * AuthorizeRequest.cpp : AuthorizeRequest implementation
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

#include "AuthorizeRequest.h"

#include "CloudProvider/CloudProvider.h"

namespace cloudstorage {

AuthorizeRequest::AuthorizeRequest(std::shared_ptr<CloudProvider> p,
                                   AuthorizationFlow callback)
    : Request(p), awaiting_authorization_code_(), callback_(callback) {
  if (!provider()->auth_callback())
    throw std::logic_error("CloudProvider's callback can't be null.");
  set_resolver([this](Request*) -> EitherError<void> {
    auto result = callback_ ? callback_(this) : oauth2Authorization();
    provider()->set_authorization_result(result);
    provider()->set_authorization_status(
        CloudProvider::AuthorizationStatus::Done);
    provider()->authorized_condition().notify_all();
    provider()->auth_callback()->done(*provider(), result);
    return result;
  });
}

AuthorizeRequest::~AuthorizeRequest() { cancel(); }

void AuthorizeRequest::cancel() {
  if (is_cancelled()) return;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (awaiting_authorization_code_) {
      auto request =
          provider()->http()->create(provider()->auth()->redirect_uri(), "GET");
      request->setParameter("accepted", "false");
      request->setParameter("state", provider()->auth()->state());
      request->setParameter("error", "cancelled");
      std::stringstream input, output;
      request->send(input, output);
    }
  }
  Request::cancel();
}

EitherError<void> AuthorizeRequest::oauth2Authorization() {
  IAuth* auth = provider()->auth();
  std::stringstream input, output, error_stream;
  IHttpRequest::Pointer r = auth->refreshTokenRequest(input);
  int code = send(r.get(), input, output, &error_stream);
  if (IHttpRequest::isSuccess(code)) {
    std::unique_lock<std::mutex> lock(provider()->auth_mutex());
    auth->set_access_token(auth->refreshTokenResponse(output));
    return nullptr;
  } else if (r && !IHttpRequest::isClientError(code))
    return Error{code, error_stream.str()};

  if (is_cancelled()) return Error{IHttpRequest::Aborted, ""};
  if (provider()->auth_callback()->userConsentRequired(*provider()) ==
      ICloudProvider::IAuthCallback::Status::WaitForAuthorizationCode) {
    auto authorization_code = getAuthorizationCode();
    if (authorization_code.left()) return authorization_code.left();
    auth->set_authorization_code(*authorization_code.right());
    std::stringstream input, output;
    std::stringstream error_stream;
    IHttpRequest::Pointer r = auth->exchangeAuthorizationCodeRequest(input);
    if (IHttpRequest::isSuccess(send(r.get(), input, output, &error_stream))) {
      std::unique_lock<std::mutex> lock(provider()->auth_mutex());
      auth->set_access_token(auth->exchangeAuthorizationCodeResponse(output));
      return nullptr;
    }
  }
  return Error{code, error_stream.str()};
}

EitherError<std::string> AuthorizeRequest::getAuthorizationCode() {
  if (is_cancelled()) return Error{IHttpRequest::Aborted, ""};
  std::unique_lock<std::mutex> lock(mutex_);
  return provider()->auth()->requestAuthorizationCode(
      [this, &lock]() {
        awaiting_authorization_code_ = true;
        lock.unlock();
      },
      [this, &lock]() {
        lock.lock();
        awaiting_authorization_code_ = false;
      });
}

}  // namespace cloudstorage
