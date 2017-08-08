/*****************************************************************************
 * Auth.cpp : Auth implementation
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

#include "Auth.h"

#include <json/json.h>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <sstream>

#include "Utility.h"

const char* DEFAULT_REDIRECT_URI = "http://localhost:12345";

const std::string CDN =
    "<script src='https://code.jquery.com/jquery-3.1.0.min.js'"
    "integrity='sha256-cCueBR6CsyA4/9szpPfrX3s49M9vUU5BgtiJj06wt/s='"
    "crossorigin='anonymous'></script>"
    "<script "
    "src='https://cdnjs.cloudflare.com/ajax/libs/js-url/2.5.0/url.min.js'>"
    "</script>";

const std::string DEFAULT_LOGIN_PAGE =
    "<html>" + CDN +
    "<body>"
    "libcloudstorage login page"
    "<table>"
    "<tr><td>Login:</td><td><input id='login'></td></tr>"
    "<tr><td>Password:</td><td><input id='password' type='password'></td></tr>"
    "<tr><td>"
    "  <a id='link'><input id='submit' type='button' value='Login'></a>"
    "</td></tr>"
    "<script>"
    " $(function() {"
    "   var func = function() {"
    "     var str = $('#login').val() + '" +
    std::string(cloudstorage::Auth::SEPARATOR) +
    "' + $('#password').val();"
    "     $('#link').attr('href', location.pathname + '?code='"
    "                     + encodeURIComponent(str) + "
    "                     '&state=' +  url('?').state);"
    "   };"
    "   $('#login').change(func);"
    "   $('#password').change(func);"
    " });"
    "</script>"
    "</table>"
    "</body>"
    "</html>";

const std::string DEFAULT_SUCCESS_PAGE =
    "<html>" + CDN +
    "<body>Success.</body>"
    "<script>"
    "  $.ajax({ 'data': { 'accepted': 'true' } });"
    "  history.replaceState({}, null, "
    "location.pathname.split(\"/\").slice(0,-1).join(\"/\") + "
    "'/success');"
    "</script>"
    "</html>";

const std::string DEFAULT_ERROR_PAGE =
    "<html>" + CDN +
    "<body>Error.</body>"
    "<script>"
    "  $.ajax({ 'data': { 'accepted': 'false' } });"
    "  history.replaceState({}, null,"
    "location.pathname.split(\"/\").slice(0,-1).join(\"/\") + "
    "'/error');"
    "</script>"
    "</html>";

namespace cloudstorage {

IHttpServer::IResponse::Pointer Auth::HttpServerCallback::receivedConnection(
    const IHttpServer& server, IHttpServer::IConnection::Pointer connection) {
  const char* state = connection->getParameter(data_.state_parameter_name_);
  if (!state || state != data_.state_)
    return server.createResponse(
        IHttpRequest::Unauthorized, {},
        data_.error_page_.empty() ? DEFAULT_ERROR_PAGE : data_.error_page_);

  const char* accepted = connection->getParameter("accepted");
  const char* code = connection->getParameter(data_.code_parameter_name_);
  const char* error = connection->getParameter(data_.error_parameter_name_);
  if (accepted && data_.callback_) {
    if (std::string(accepted) == "true" && code) {
      data_.callback_(std::string(code));
    } else {
      if (error)
        data_.callback_(Error{IHttpRequest::Bad, error});
      else
        data_.callback_(Error{IHttpRequest::Bad, ""});
    }
    data_.callback_ = nullptr;
  }

  if (code)
    return server.createResponse(IHttpRequest::Ok, {},
                                 data_.success_page_.empty()
                                     ? DEFAULT_SUCCESS_PAGE
                                     : data_.success_page_);

  if (error)
    return server.createResponse(
        IHttpRequest::Unauthorized, {},
        data_.error_page_.empty() ? DEFAULT_ERROR_PAGE : data_.error_page_);

  if (connection->url() == data_.redirect_uri_path_ + "/login")
    return server.createResponse(
        IHttpRequest::Ok, {},
        data_.login_page_.empty() ? DEFAULT_LOGIN_PAGE : data_.login_page_);

  return server.createResponse(
      IHttpRequest::NotFound, {},
      data_.error_page_.empty() ? DEFAULT_ERROR_PAGE : data_.error_page_);
}

Auth::Auth() : redirect_uri_(DEFAULT_REDIRECT_URI), http_(), http_server_() {}

void Auth::initialize(IHttp* http, IHttpServerFactory* factory) {
  http_ = http;
  http_server_ = factory;
}

const std::string& Auth::authorization_code() const {
  return authorization_code_;
}

void Auth::set_authorization_code(const std::string& code) {
  authorization_code_ = code;
}

const std::string& Auth::client_id() const { return client_id_; }

void Auth::set_client_id(const std::string& client_id) {
  client_id_ = client_id;
}

const std::string& Auth::client_secret() const { return client_secret_; }

void Auth::set_client_secret(const std::string& client_secret) {
  client_secret_ = client_secret;
}

std::string Auth::redirect_uri() const { return redirect_uri_; }

void Auth::set_redirect_uri(const std::string& uri) { redirect_uri_ = uri; }

std::string Auth::redirect_uri_path() const {
  const char* http = "http://";
  int cnt = std::count(http, http + strlen(http), '/') + 1;
  std::string str = redirect_uri();
  for (size_t i = 0; i < str.length(); i++) {
    if (str[i] == '/') cnt--;
    if (cnt == 0) return std::string(str.begin() + i, str.end());
  }
  return "";
}

std::string Auth::state() const { return state_; }

void Auth::set_state(const std::string& state) { state_ = state; }

std::string Auth::login_page() const { return login_page_; }
void Auth::set_login_page(const std::string& page) { login_page_ = page; }

std::string Auth::success_page() const { return success_page_; }
void Auth::set_success_page(const std::string& page) { success_page_ = page; }

std::string Auth::error_page() const { return error_page_; }
void Auth::set_error_page(const std::string& page) { error_page_ = page; }

Auth::Token* Auth::access_token() const { return access_token_.get(); }

void Auth::set_access_token(Token::Pointer token) {
  access_token_ = std::move(token);
}

IHttp* Auth::http() const { return http_; }

IHttpServer::Pointer Auth::awaitAuthorizationCode(
    std::string code_parameter_name, std::string error_parameter_name,
    std::string state_parameter_name, CodeReceived complete) const {
  auto callback = std::make_shared<HttpServerCallback>();
  callback->data_ = {code_parameter_name,
                     error_parameter_name,
                     state_parameter_name,
                     redirect_uri_path(),
                     state(),
                     login_page(),
                     success_page(),
                     error_page(),
                     complete};
  auto current_server =
      http_server_->create(callback, state(), IHttpServer::Type::Authorization);
  if (!current_server)
    complete(Error{IHttpRequest::Failure, "couldn't start http server"});
  return current_server;
}

IHttpServer::Pointer Auth::requestAuthorizationCode(CodeReceived c) const {
  return awaitAuthorizationCode("code", "error", "state", c);
}

IAuth::Token::Pointer Auth::fromTokenString(
    const std::string& refresh_token) const {
  Token::Pointer token = util::make_unique<Token>();
  token->refresh_token_ = refresh_token;
  token->expires_in_ = -1;
  return token;
}

}  // namespace cloudstorage
