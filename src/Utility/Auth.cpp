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
#include <fstream>
#include <sstream>

#include "Utility.h"

const uint16_t DEFAULT_REDIRECT_URI_PORT = 12345;

const std::string JQUERY =
    "<script src=\"https://code.jquery.com/jquery-3.1.0.min.js\""
    "integrity=\"sha256-cCueBR6CsyA4/9szpPfrX3s49M9vUU5BgtiJj06wt/s=\""
    "crossorigin=\"anonymous\"></script>";

const std::string LOGIN_PAGE =
    "<body>"
    "libcloudstorage login page"
    "<table>"
    "<tr><td>Login:</td><td><input id='login'></td></tr>"
    "<tr><td>Password:</td><td><input id='password' type='password'></td></tr>"
    "<tr><td><input id='submit' type='button' value='Login'></td></tr>"
    "<script>"
    " $(function() {"
    "   $('#submit').click(function() {"
    "     $.ajax({"
    "       url: '/',"
    "       method: 'GET',"
    "       data: {"
    "         'code' : $('#login').val() + '" +
    std::string(cloudstorage::Auth::SEPARATOR) +
    "' + $('#password').val(),"
    "         'accepted' : 'true'"
    "       }"
    "     });"
    "   })"
    " });"
    "</script>"
    "</table>"
    "</body>";

namespace cloudstorage {
namespace {

std::string sendHttpRequestFromJavaScript(const Json::Value& json) {
  std::stringstream stream;
  stream << "<script>$.ajax(" << json << ")</script>";
  return stream.str();
}

}  // namespace

IHttpServer::IResponse::Pointer Auth::HttpServerCallback::receivedConnection(
    const IHttpServer& server, const IHttpServer::IConnection& connection) {
  std::string page = JQUERY;

  if (connection.url() == "/login") page += LOGIN_PAGE;

  const char* code = connection.getParameter(data_.code_parameter_name_);
  if (code) {
    data_.code_ = code;
    Json::Value json;
    json["data"]["accepted"] = "true";
    page += "<body>Success.</body>" + sendHttpRequestFromJavaScript(json);
  }

  const char* error = connection.getParameter(data_.error_parameter_name_);
  if (error) {
    Json::Value json;
    json["data"]["accepted"] = "false";
    page +=
        "<body>Error occurred.</body>" + sendHttpRequestFromJavaScript(json);
  }

  const char* accepted = connection.getParameter("accepted");
  if (accepted) {
    if (std::string(accepted) == "true") {
      data_.state_ = HttpServerData::Accepted;
    } else
      data_.state_ = HttpServerData::Denied;
    data_.semaphore_->notify();
  }

  return server.createResponse(200, {}, page);
}

Auth::Auth()
    : redirect_uri_port_(DEFAULT_REDIRECT_URI_PORT), http_(), http_server_() {}

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

std::string Auth::redirect_uri() const {
  return "http://localhost:" + std::to_string(redirect_uri_port());
}

uint16_t Auth::redirect_uri_port() const { return redirect_uri_port_; }

void Auth::set_redirect_uri_port(uint16_t port) { redirect_uri_port_ = port; }

Auth::Token* Auth::access_token() const { return access_token_.get(); }

void Auth::set_access_token(Token::Pointer token) {
  access_token_ = std::move(token);
}

IHttp* Auth::http() const { return http_; }

std::string Auth::awaitAuthorizationCode(
    std::string code_parameter_name, std::string error_parameter_name,
    std::function<void()> server_started,
    std::function<void()> server_stopped) const {
  uint16_t http_server_port = redirect_uri_port();
  Semaphore semaphore;
  auto callback = std::make_shared<HttpServerCallback>();
  callback->data_ = {"",
                     code_parameter_name,
                     error_parameter_name,
                     http_server_port,
                     HttpServerCallback::HttpServerData::Awaiting,
                     &semaphore};
  {
    auto http_server = http_server_->create(
        callback, IHttpServer::Type::SingleThreaded, http_server_port);
    if (server_started) server_started();
    semaphore.wait();
  }
  if (server_stopped) server_stopped();
  if (callback->data_.state_ == HttpServerCallback::HttpServerData::Accepted)
    return callback->data_.code_;
  else
    return "";
}

std::string Auth::requestAuthorizationCode(
    std::function<void()> server_started,
    std::function<void()> server_stopped) const {
  return awaitAuthorizationCode("code", "error", server_started,
                                server_stopped);
}

IAuth::Token::Pointer Auth::fromTokenString(
    const std::string& refresh_token) const {
  Token::Pointer token = util::make_unique<Token>();
  token->refresh_token_ = refresh_token;
  token->expires_in_ = -1;
  return token;
}

}  // namespace cloudstorage
