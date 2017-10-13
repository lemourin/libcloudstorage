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

#include <mutex>

namespace cloudstorage {

class Auth : public IAuth {
 public:
  class HttpServerCallback : public IHttpServer::ICallback {
   public:
    IHttpServer::IResponse::Pointer handle(
        const IHttpServer::IRequest&) override;

    struct HttpServerData {
      std::string code_parameter_name_;
      std::string error_parameter_name_;
      std::string state_parameter_name_;
      std::string redirect_uri_path_;
      std::string state_;
      std::string login_page_;
      std::string success_page_;
      std::string error_page_;
      CodeReceived callback_;
    } data_;

    std::mutex lock_;
  };

  Auth();

  void initialize(IHttp*, IHttpServerFactory*) override;

  const std::string& authorization_code() const override;
  void set_authorization_code(const std::string&) override;

  const std::string& client_id() const override;
  void set_client_id(const std::string&) override;

  const std::string& client_secret() const override;
  void set_client_secret(const std::string&) override;

  std::string redirect_uri() const override;
  void set_redirect_uri(const std::string&) override;

  std::string state() const override;
  void set_state(const std::string&) override;

  std::string login_page() const override;
  void set_login_page(const std::string&) override;

  std::string success_page() const override;
  void set_success_page(const std::string&) override;

  std::string error_page() const override;
  void set_error_page(const std::string&) override;

  Token* access_token() const override;
  void set_access_token(Token::Pointer) override;

  ICloudProvider::Permission permission() const override;
  void set_permission(ICloudProvider::Permission) override;

  IHttp* http() const override;

  IHttpServer::Pointer awaitAuthorizationCode(std::string code_parameter_name,
                                              std::string error_parameter_name,
                                              std::string state_parameter_name,
                                              CodeReceived) const override;

  IHttpServer::Pointer requestAuthorizationCode(CodeReceived) const override;

  Token::Pointer fromTokenString(const std::string&) const override;

  std::string redirect_uri_path() const;

 private:
  std::string authorization_code_;
  std::string client_id_;
  std::string client_secret_;
  std::string redirect_uri_;
  std::string state_;
  std::string login_page_;
  std::string success_page_;
  std::string error_page_;
  ICloudProvider::Permission permission_;
  Token::Pointer access_token_;
  IHttp* http_;
  IHttpServerFactory* http_server_;
};

}  // namespace cloudstorage

#endif  // AUTH_H
