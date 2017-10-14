/*****************************************************************************
 * ExchangeCodeRequest.cpp
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

#include "ExchangeCodeRequest.h"

#include "CloudProvider/CloudProvider.h"

namespace cloudstorage {

ExchangeCodeRequest::ExchangeCodeRequest(std::shared_ptr<CloudProvider> p,
                                         const std::string& authorization_code,
                                         ExchangeCodeCallback callback)
    : Request(p, callback, [=](Request<EitherError<Token>>::Pointer r) {
        std::stringstream stream;
        if (!provider()->auth()->exchangeAuthorizationCodeRequest(stream))
          return r->done(Token{authorization_code, ""});
        r->send(
            [=](util::Output input) {
              auto lock = provider()->auth_lock();
              auto previous_code = provider()->auth()->authorization_code();
              provider()->auth()->set_authorization_code(authorization_code);
              auto request =
                  provider()->auth()->exchangeAuthorizationCodeRequest(*input);
              provider()->auth()->set_authorization_code(previous_code);
              return request;
            },
            [=](EitherError<Response> e) {
              if (e.left()) return r->done(e.left());
              try {
                auto lock = provider()->auth_lock();
                auto auth_token =
                    provider()->auth()->exchangeAuthorizationCodeResponse(
                        e.right()->output());
                lock.unlock();
                Token token{auth_token->refresh_token_, auth_token->token_};
                r->done(token);
              } catch (const std::exception&) {
                r->done(
                    Error{IHttpRequest::Failure, e.right()->output().str()});
              }
            });
      }) {}

ExchangeCodeRequest::~ExchangeCodeRequest() { cancel(); }

}  // namespace cloudstorage
