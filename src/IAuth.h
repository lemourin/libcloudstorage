/*****************************************************************************
 * IAuth.h : interface of IAuth
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

#ifndef IAUTH_H
#define IAUTH_H

#include <memory>
#include <string>

#include "ICloudProvider.h"
#include "IHttpServer.h"

namespace cloudstorage {

class IAuth {
 public:
  using Pointer = std::unique_ptr<IAuth>;
  using CodeReceived = std::function<void(EitherError<std::string>)>;

  struct Token {
    using Pointer = std::unique_ptr<Token>;

    std::string token_;
    std::string refresh_token_;
    int expires_in_;
  };

  virtual ~IAuth() = default;

  virtual void initialize(IHttp*, IHttpServerFactory*) = 0;
  virtual IHttp* http() const = 0;

  virtual const std::string& authorization_code() const = 0;
  virtual void set_authorization_code(const std::string&) = 0;

  virtual const std::string& client_id() const = 0;
  virtual void set_client_id(const std::string&) = 0;

  virtual const std::string& client_secret() const = 0;
  virtual void set_client_secret(const std::string&) = 0;

  virtual std::string redirect_uri() const = 0;
  virtual void set_redirect_uri(const std::string&) = 0;

  virtual ICloudProvider::Permission permission() const = 0;
  virtual void set_permission(ICloudProvider::Permission) = 0;

  virtual std::string state() const = 0;
  virtual void set_state(const std::string&) = 0;

  virtual std::string login_page() const = 0;
  virtual void set_login_page(const std::string&) = 0;

  virtual std::string success_page() const = 0;
  virtual void set_success_page(const std::string&) = 0;

  virtual std::string error_page() const = 0;
  virtual void set_error_page(const std::string&) = 0;

  virtual Token* access_token() const = 0;
  virtual void set_access_token(Token::Pointer) = 0;

  virtual std::string authorizeLibraryUrl() const = 0;

  /**
   * Creates a web server which fires a callback when it receives a
   * get request with either code_parameter_name or error_parameter_name.
   *
   * @param code_parameter_name usually "code"
   * @param error_parameter_name usually "error"
   * @param state_parameter_name usually "state"
   */
  virtual IHttpServer::Pointer awaitAuthorizationCode(
      std::string code_parameter_name, std::string error_parameter_name,
      std::string state_parameter_name, CodeReceived) const = 0;

  /**
   * Shortcut for awaitAuthorizationCode, usually calls
   * awaitAuthorizationCode("code", "error").
   *
   * @return authorization code
   */
  virtual IHttpServer::Pointer requestAuthorizationCode(CodeReceived) const = 0;

  virtual Token::Pointer fromTokenString(const std::string&) const = 0;

  /**
   * Should create a request which retrieves refresh token after receiving
   * authorization_code.
   *
   * @param input_data data to be sent in request body
   * @return http request
   */
  virtual IHttpRequest::Pointer exchangeAuthorizationCodeRequest(
      std::ostream& input_data) const = 0;

  /**
   * Should create a request which obtains access token from refresh token.
   *
   * @param input_data data to be sent in request body
   * @return http request
   */
  virtual IHttpRequest::Pointer refreshTokenRequest(
      std::ostream& input_data) const = 0;

  /**
   * Should translate response to exchangeAuthorizationCodeRequest into Token
   * object.
   *
   * @param response
   * @return token
   */
  virtual Token::Pointer exchangeAuthorizationCodeResponse(
      std::istream& response) const = 0;

  /**
   * Should translate response to refreshTokenResponse into Token object.
   *
   * @param response
   * @return refreshed token
   */
  virtual Token::Pointer refreshTokenResponse(std::istream& response) const = 0;
};

}  // namespace cloudstorage

#endif  // IAUTH_H
