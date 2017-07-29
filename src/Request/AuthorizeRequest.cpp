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
  if (!provider()->callback())
    throw std::logic_error("CloudProvider's callback can't be null.");
  set_resolver([this](Request*) -> EitherError<void> {
    auto result = callback_ ? callback_(this) : oauth2Authorization();
    provider()->set_authorization_status(
        !result.left() ? CloudProvider::AuthorizationStatus::Success
                       : CloudProvider::AuthorizationStatus::Fail);
    provider()->authorized_condition().notify_all();
    if (!result.left())
      provider()->callback()->accepted(*provider());
    else
      provider()->callback()->declined(*provider());
    return result;
  });
  set_cancel_callback([this]() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (awaiting_authorization_code_) {
      auto request =
          provider()->http()->create(provider()->auth()->redirect_uri(), "GET");
      request->setParameter("accepted", "false");
      request->setParameter("state", provider()->auth()->state());
      std::stringstream input, output;
      request->send(input, output);
    }
  });
}

AuthorizeRequest::~AuthorizeRequest() { cancel(); }

EitherError<void> AuthorizeRequest::oauth2Authorization() {
  IAuth* auth = provider()->auth();
  std::stringstream input, output, error_stream;
  IHttpRequest::Pointer r = auth->refreshTokenRequest(input);
  int code = send(r.get(), input, output, &error_stream);
  if (IHttpRequest::isSuccess(code)) {
    std::unique_lock<std::mutex> lock(provider()->auth_mutex());
    auth->set_access_token(auth->refreshTokenResponse(output));
    return nullptr;
  } else if (r && !IHttpRequest::isClientError(code)) {
    if (!is_cancelled())
      provider()->callback()->error(*provider(),
                                    error_string(code, error_stream.str()));
    return Error{code, error_stream.str()};
  }

  if (is_cancelled()) return Error{IHttpRequest::Aborted, ""};
  if (provider()->callback()->userConsentRequired(*provider()) ==
      ICloudProvider::ICallback::Status::WaitForAuthorizationCode) {
    std::string authorization_code = getAuthorizationCode();
    if (authorization_code.empty())
      return Error{500, "failed to get authorization code"};
    auth->set_authorization_code(authorization_code);
    std::stringstream input, output;
    std::stringstream error_stream;
    IHttpRequest::Pointer r = auth->exchangeAuthorizationCodeRequest(input);
    if (IHttpRequest::isSuccess(send(r.get(), input, output, &error_stream))) {
      std::unique_lock<std::mutex> lock(provider()->auth_mutex());
      auth->set_access_token(auth->exchangeAuthorizationCodeResponse(output));
      return nullptr;
    } else if (!is_cancelled()) {
      provider()->callback()->error(*provider(),
                                    error_string(code, error_stream.str()));
    }
  }
  return Error{code, error_stream.str()};
}

std::string AuthorizeRequest::getAuthorizationCode() {
  std::unique_lock<std::mutex> lock(mutex_);
  if (is_cancelled()) return "";
  std::string authorization_code = provider()->auth()->requestAuthorizationCode(
      [this, &lock]() {
        awaiting_authorization_code_ = true;
        lock.unlock();
      },
      [this, &lock]() {
        lock.lock();
        awaiting_authorization_code_ = false;
      });
  return authorization_code;
}

}  // namespace cloudstorage
