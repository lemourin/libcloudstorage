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
    : Request(p) {
  set_resolver(
      [this, authorization_code, callback](
          Request<EitherError<std::string>>* r) -> EitherError<std::string> {
        std::stringstream stream;
        if (!provider()->auth()->exchangeAuthorizationCodeRequest(stream)) {
          callback(authorization_code);
          return authorization_code;
        }
        std::stringstream input, output, error;
        IHttpRequest::Pointer request;
        {
          std::lock_guard<std::mutex> lock(provider()->auth_mutex());
          auto previous_code = provider()->auth()->authorization_code();
          provider()->auth()->set_authorization_code(authorization_code);
          request = provider()->auth()->exchangeAuthorizationCodeRequest(input);
          provider()->auth()->set_authorization_code(previous_code);
        }
        int code = r->send(request.get(), input, output, &error);
        if (IHttpRequest::isSuccess(code)) {
          std::lock_guard<std::mutex> lock(provider()->auth_mutex());
          auto token = provider()
                           ->auth()
                           ->exchangeAuthorizationCodeResponse(output)
                           ->refresh_token_;
          callback(token);
          return token;
        } else {
          Error e{code, error.str()};
          callback(e);
          return e;
        }
      });
}

ExchangeCodeRequest::~ExchangeCodeRequest() { cancel(); }

}  // namespace cloudstorage
