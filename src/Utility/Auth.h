/*****************************************************************************
 * Auth.h : Auth prototypes
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

#ifndef AUTH_H
#define AUTH_H

#include "IAuth.h"

namespace cloudstorage {

class AuthorizationException : public std::exception {
 public:
  const char* what() const noexcept { return "Authorization error."; }
};

class Auth : public IAuth {
 public:
  static constexpr const char* SEPARATOR = "##";

  Auth();

  void initialize(IHttp*);

  const std::string& authorization_code() const;
  void set_authorization_code(const std::string&);

  const std::string& client_id() const;
  void set_client_id(const std::string&);

  const std::string& client_secret() const;
  void set_client_secret(const std::string&);

  std::string redirect_uri() const;

  uint16_t redirect_uri_port() const;
  void set_redirect_uri_port(uint16_t);

  Token* access_token() const;
  void set_access_token(Token::Pointer);

  IHttp* http() const;

  std::string awaitAuthorizationCode(
      std::string code_parameter_name, std::string error_parameter_name,
      std::function<void()> server_started = nullptr,
      std::function<void()> server_stopped = nullptr) const;

  std::string requestAuthorizationCode(
      std::function<void()> server_started = nullptr,
      std::function<void()> server_stopped = nullptr) const;

  Token::Pointer fromTokenString(const std::string&) const;

 private:
  std::string authorization_code_;
  std::string client_id_;
  std::string client_secret_;
  uint16_t redirect_uri_port_;
  Token::Pointer access_token_;
  IHttp* http_;
};

}  // namespace cloudstorage

#endif  // AUTH_H
