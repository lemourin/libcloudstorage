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
#include "Utility/HttpRequest.h"

#include <iostream>
namespace cloudstorage {

AuthorizeRequest::AuthorizeRequest(std::shared_ptr<CloudProvider> p,
                                   Callback callback)
    : Request(p), awaiting_authorization_code_(), callback_(callback) {
  if (!provider()->callback())
    throw std::logic_error("CloudProvider's callback can't be null.");
  set_resolver([this](Request*) {
    bool success = authorize();
    if (callback_) success = callback_(success, this);
    provider()->set_authorization_status(
        success ? CloudProvider::AuthorizationStatus::Success
                : CloudProvider::AuthorizationStatus::Fail);
    provider()->authorized_condition().notify_all();
    if (success)
      provider()->callback()->accepted(*provider());
    else
      provider()->callback()->declined(*provider());
    return success;
  });
}

AuthorizeRequest::~AuthorizeRequest() { cancel(); }

void AuthorizeRequest::cancel() {
  set_cancelled(true);
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (awaiting_authorization_code_) {
      HttpRequest request(provider()->auth()->redirect_uri(),
                          HttpRequest::Type::GET);
      request.setParameter("accepted", "false");
      try {
        request.send();
      } catch (const HttpException& e) {
        if (e.code() != CURLE_GOT_NOTHING) throw;
      }
    }
  }
  finish();
}

bool AuthorizeRequest::authorize() {
  IAuth* auth = provider()->auth();
  std::stringstream input, output, error_stream;
  HttpRequest::Pointer r = auth->refreshTokenRequest(input);
  int code = send(r.get(), input, output, &error_stream);
  if (HttpRequest::isSuccess(code)) {
    std::unique_lock<std::mutex> lock(provider()->auth_mutex());
    auth->set_access_token(auth->refreshTokenResponse(output));
    return true;
  } else if (r && !HttpRequest::isClientError(code)) {
    if (!is_cancelled())
      provider()->callback()->error(*provider(),
                                    error_string(code, error_stream.str()));
    return false;
  }

  if (is_cancelled()) return false;
  if (provider()->callback()->userConsentRequired(*provider()) ==
      ICloudProvider::ICallback::Status::WaitForAuthorizationCode) {
    {
      std::unique_lock<std::mutex> lock(mutex_);
      if (is_cancelled()) return false;
      std::string authorization_code = auth->requestAuthorizationCode(
          [this, &lock]() {
            awaiting_authorization_code_ = true;
            lock.unlock();
          },
          [this, &lock]() {
            lock.lock();
            awaiting_authorization_code_ = false;
          });
      if (authorization_code.empty()) return false;
      auth->set_authorization_code(authorization_code);
    }
    std::stringstream input, output;
    std::stringstream error_stream;
    HttpRequest::Pointer r = auth->exchangeAuthorizationCodeRequest(input);
    if (HttpRequest::isSuccess(send(r.get(), input, output, &error_stream))) {
      std::unique_lock<std::mutex> lock(provider()->auth_mutex());
      auth->set_access_token(auth->exchangeAuthorizationCodeResponse(output));
      return true;
    } else if (!is_cancelled()) {
      provider()->callback()->error(*provider(),
                                    error_string(code, error_stream.str()));
    }
  }
  return false;
}

}  // namespace cloudstorage
