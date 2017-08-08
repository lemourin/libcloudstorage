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
                                   AuthorizeCompleted complete,
                                   AuthorizationFlow callback)
    : Request(p) {
  if (!provider()->auth_callback())
    throw std::logic_error("CloudProvider's callback can't be null.");
  set([=](Request::Ptr request) {
    auto on_complete = [=](EitherError<void> result) {
      complete(result);
      request->done(result);
      provider()->auth_callback()->done(*provider(), result);
      std::unique_lock<std::mutex> lock(
          provider()->current_authorization_mutex_);
      for (auto&& c : provider()->auth_callbacks_)
        for (auto&& r : c.second) r(result);
      provider()->auth_callbacks_.clear();
      provider()->current_authorization_ = nullptr;
    };
    callback ? callback(std::static_pointer_cast<AuthorizeRequest>(request),
                        on_complete)
             : oauth2Authorization(on_complete);
  });
}

AuthorizeRequest::~AuthorizeRequest() { cancel(); }

void AuthorizeRequest::oauth2Authorization(AuthorizeCompleted complete) {
  auto p = provider();
  auto input = std::make_shared<std::stringstream>(),
       output = std::make_shared<std::stringstream>(),
       error_stream = std::make_shared<std::stringstream>();
  auto r = p->auth()->refreshTokenRequest(*input);
  send(r.get(),
       [=](int code, util::Output, util::Output) {
         if (is_cancelled()) {
           Error e{IHttpRequest::Aborted, ""};
           return complete(e);
         }
         if (IHttpRequest::isSuccess(code)) {
           {
             std::unique_lock<std::mutex> lock(p->auth_mutex());
             p->auth()->set_access_token(
                 p->auth()->refreshTokenResponse(*output));
           }
           return complete(nullptr);
         } else if (!IHttpRequest::isClientError(code)) {
           return complete(Error{code, error_stream->str()});
         }
         if (p->auth_callback()->userConsentRequired(*provider()) ==
             ICloudProvider::IAuthCallback::Status::WaitForAuthorizationCode) {
           p->auth()->requestAuthorizationCode(
               [=](EitherError<std::string> authorization_code) {
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
                            std::unique_lock<std::mutex> lock(p->auth_mutex());
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
               });
         } else {
           complete(Error{code, error_stream->str()});
         }
       },
       input, output, error_stream);
}

SimpleAuthorization::SimpleAuthorization(std::shared_ptr<CloudProvider> p,
                                         AuthorizeCompleted c)
    : AuthorizeRequest(p, c, [=](AuthorizeRequest::Ptr r,
                                 AuthorizeRequest::AuthorizeCompleted
                                     complete) {
        if (p->auth_callback()->userConsentRequired(*p) !=
            ICloudProvider::IAuthCallback::Status::WaitForAuthorizationCode) {
          return complete(Error{IHttpRequest::Failure, "not waiting for code"});
        }
        r->provider()->auth()->requestAuthorizationCode(
            [=](EitherError<std::string> code) {
              (void)r;
              if (code.left())
                complete(code.left());
              else
                complete(p->unpackCredentials(*code.right())
                             ? EitherError<void>(nullptr)
                             : EitherError<void>(Error{IHttpRequest::Failure,
                                                       "invalid code"}));
            });
      }) {}

}  // namespace cloudstorage
