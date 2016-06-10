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

namespace cloudstorage {

class IAuth {
 public:
  using Pointer = std::unique_ptr<IAuth>;

  struct Token {
    using Pointer = std::unique_ptr<Token>;

    std::string token_;
    std::string refresh_token_;
    int expires_in_;
  };

  class ICallback {
   public:
    using Pointer = std::unique_ptr<ICallback>;

    enum class Status {
      WaitForAuthorizationCode,
      None
    };

    virtual ~ICallback() = default;
    virtual Status userConsentRequired(const IAuth&) const = 0;
  };

  virtual ~IAuth() = default;

  virtual bool authorize(ICallback*) = 0;

  virtual const std::string& authorization_code() const = 0;
  virtual void set_authorization_code(const std::string&) = 0;

  virtual const std::string& client_id() const = 0;
  virtual void set_client_id(const std::string&) = 0;

  virtual const std::string& client_secret() const = 0;
  virtual void set_client_secret(const std::string&) = 0;

  virtual std::string redirect_uri() const = 0;

  virtual uint16_t redirect_uri_port() const = 0;
  virtual void set_redirect_uri_port(uint16_t) = 0;

  virtual Token* access_token() const = 0;
  virtual void set_access_token(Token::Pointer) = 0;

  virtual std::string authorizeLibraryUrl() const = 0;

  virtual std::string awaitAuthorizationCode(
      std::string code_parameter_name,
      std::string error_parameter_name) const = 0;

  virtual std::string requestAuthorizationCode() const = 0;

  virtual Token::Pointer requestAccessToken() const = 0;
  virtual Token::Pointer refreshToken() const = 0;

  virtual bool validateToken(Token&) const = 0;
};

}  // namespace cloudstorage

#endif  // IAUTH_H
