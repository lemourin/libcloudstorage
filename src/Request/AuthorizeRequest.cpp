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
    : Request(p) {
  if (!provider()->auth_callback())
    throw std::logic_error("CloudProvider's callback can't be null.");
  set([=](Request::Ptr request) {
    auto on_complete = [=](EitherError<void> result) {
      request->done(result);
      provider()->auth_callback()->done(*provider(), result);
      std::unique_lock<std::mutex> lock(
          provider()->current_authorization_mutex_);
      while (!provider()->auth_callbacks_.empty()) {
        {
          auto v = std::move(provider()->auth_callbacks_.begin()->second);
          provider()->auth_callbacks_.erase(
              provider()->auth_callbacks_.begin());
          lock.unlock();
          for (auto&& r : v) r(result);
        }
        lock.lock();
      }
      provider()->current_authorization_ = nullptr;
    };
    callback ? callback(std::static_pointer_cast<AuthorizeRequest>(request),
                        on_complete)
             : oauth2Authorization(on_complete);
  });
}

AuthorizeRequest::~AuthorizeRequest() { cancel(); }

void AuthorizeRequest::sendCancel() {
  auto request =
      provider()->http()->create(provider()->auth()->redirect_uri(), "GET");
  request->setParameter("accepted", "false");
  request->setParameter("state", provider()->auth()->state());
  request->setParameter("error", "cancelled");
  std::shared_ptr<IHttpServer> auth_server;
  {
    std::lock_guard<std::mutex> lock(lock_);
    auth_server = auth_server_;
  }
  if (auth_server) {
    auto output = std::make_shared<std::stringstream>(),
         error = std::make_shared<std::stringstream>();
    request->send(
        [=](int code, util::Output, util::Output) {
          (void)auth_server;
          if (code != IHttpRequest::Unauthorized)
            throw std::runtime_error("couldn't cancel authorize request");
        },
        output, error);
  }
}

void AuthorizeRequest::cancel() {
  if (is_cancelled()) return;
  sendCancel();
  Request::cancel();
}

void AuthorizeRequest::set_server(std::shared_ptr<IHttpServer> p,
                                  AuthorizeCompleted complete) {
  {
    std::lock_guard<std::mutex> lock(lock_);
    auth_server_ = p;
  }
  if (p) {
    if (is_cancelled()) sendCancel();
  } else {
    complete(Error{IHttpRequest::Failure, "can't start http server"});
  }
}

void AuthorizeRequest::oauth2Authorization(AuthorizeCompleted complete) {
  auto p = provider();
  auto input = std::make_shared<std::stringstream>(),
       output = std::make_shared<std::stringstream>(),
       error_stream = std::make_shared<std::stringstream>();
  auto r = p->auth()->refreshTokenRequest(*input);
  send(r.get(),
       [=](int code, util::Output, util::Output) {
         if (IHttpRequest::isSuccess(code)) {
           {
             auto lock = p->auth_lock();
             p->auth()->set_access_token(
                 p->auth()->refreshTokenResponse(*output));
           }
           return complete(nullptr);
         } else if (!IHttpRequest::isClientError(code) && r) {
           return complete(Error{code, error_stream->str()});
         }
         if (p->auth_callback()->userConsentRequired(*provider()) ==
             ICloudProvider::IAuthCallback::Status::WaitForAuthorizationCode) {
           auto code = [=](EitherError<std::string> authorization_code) {
             if (authorization_code.left())
               return complete(authorization_code.left());
             p->auth()->set_authorization_code(*authorization_code.right());
             auto input = std::make_shared<std::stringstream>(),
                  output = std::make_shared<std::stringstream>(),
                  error_stream = std::make_shared<std::stringstream>();
             auto r = p->auth()->exchangeAuthorizationCodeRequest(*input);
             send(r.get(),
                  [=](int code, util::Output, util::Output) {
                    if (IHttpRequest::isSuccess(code)) {
                      {
                        auto lock = p->auth_lock();
                        p->auth()->set_access_token(
                            p->auth()->exchangeAuthorizationCodeResponse(
                                *output));
                      }
                      complete(nullptr);
                    } else {
                      complete(Error{code, error_stream->str()});
                    }
                  },
                  input, output, error_stream);
           };
           set_server(p->auth()->requestAuthorizationCode(code), complete);
         } else {
           complete(Error{code, error_stream->str()});
         }
       },
       input, output, error_stream);
}

SimpleAuthorization::SimpleAuthorization(std::shared_ptr<CloudProvider> p)
    : AuthorizeRequest(p, [=](AuthorizeRequest::Ptr r,
                              AuthorizeRequest::AuthorizeCompleted complete) {
        if (p->auth_callback()->userConsentRequired(*p) !=
            ICloudProvider::IAuthCallback::Status::WaitForAuthorizationCode) {
          return complete(Error{IHttpRequest::Failure, "not waiting for code"});
        }
        auto code = [=](EitherError<std::string> code) {
          (void)r;
          if (code.left())
            complete(code.left());
          else
            complete(p->unpackCredentials(*code.right())
                         ? EitherError<void>(nullptr)
                         : EitherError<void>(
                               Error{IHttpRequest::Failure, "invalid code"}));
        };
        set_server(r->provider()->auth()->requestAuthorizationCode(code),
                   complete);
      }) {}

}  // namespace cloudstorage
