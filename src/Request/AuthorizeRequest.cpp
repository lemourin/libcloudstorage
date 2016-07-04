/*****************************************************************************
 * AuthorizeRequest.cpp : AuthorizeRequest implementation
 *
 *****************************************************************************
 * Copyright (C) 2016-2016 VideoLAN
 *
 * Authors: Paweł Wegner <pawel.wegner95@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "AuthorizeRequest.h"

#include "CloudProvider/CloudProvider.h"
#include "HttpCallback.h"
#include "Utility/HttpRequest.h"

namespace cloudstorage {

AuthorizeRequest::AuthorizeRequest(std::shared_ptr<CloudProvider> p)
    : Request(p), awaiting_authorization_code_() {
  if (!provider()->callback_)
    throw std::logic_error("CloudProvider's callback can't be null.");
  function_ = std::async(std::launch::async, [this]() {
    bool ret;
    {
      std::unique_lock<std::mutex> currently_authorizing(
          provider()->currently_authorizing_mutex_);
      if (provider()->currently_authorizing_) {
        provider()->authorized_.wait(currently_authorizing, [this]() {
          return !provider()->currently_authorizing_;
        });
        return provider()->current_authorization_successful_;
      }
      provider()->currently_authorizing_ = true;
      provider()->current_authorization_successful_ = false;
      currently_authorizing.unlock();
      {
        std::unique_lock<std::mutex> lock(provider()->auth_mutex_);
        ret = authorize();
      }
      currently_authorizing.lock();
      provider()->currently_authorizing_ = false;
      provider()->current_authorization_successful_ = ret;
    }
    provider()->authorized_.notify_all();
    if (ret)
      provider()->callback_->accepted(*provider());
    else
      provider()->callback_->declined(*provider());
    return ret;
  });
}

AuthorizeRequest::~AuthorizeRequest() { cancel(); }

bool AuthorizeRequest::result() {
  std::shared_future<bool> future = function_;
  if (!future.valid()) throw std::logic_error("Future invalid.");
  return future.get();
}

void AuthorizeRequest::finish() {
  if (function_.valid()) function_.get();
}

void AuthorizeRequest::cancel() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    set_cancelled(true);
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
  try {
    IAuth* auth = provider()->auth();
    if (auth->access_token()) {
      std::stringstream input, output;
      HttpRequest::Pointer r = auth->validateTokenRequest(input);
      if (HttpRequest::isSuccess(
              r->send(input, output, nullptr, httpCallback()))) {
        if (auth->validateTokenResponse(output)) return true;
      }

      input = std::stringstream();
      output = std::stringstream();
      r = auth->refreshTokenRequest(input);
      if (HttpRequest::isSuccess(
              r->send(input, output, nullptr, httpCallback()))) {
        auth->set_access_token(auth->refreshTokenResponse(output));
        return true;
      }
    }

    if (is_cancelled()) return false;
    if (provider()->callback_->userConsentRequired(*provider()) ==
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
      if (HttpRequest::isSuccess(
              r->send(input, output, &error_stream, httpCallback()))) {
        auth->set_access_token(auth->exchangeAuthorizationCodeResponse(output));
        return true;
      } else {
        provider()->callback_->error(*provider(), error_stream.str());
      }
    }
    return false;
  } catch (const HttpException& e) {
    provider()->callback_->error(*provider(), e.what());
    return false;
  }
}

}  // namespace cloudstorage
