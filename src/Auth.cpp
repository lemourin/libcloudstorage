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

#include <jsoncpp/json/json.h>
#include <microhttpd.h>
#include <fstream>
#include "Utility.h"

const uint16_t DEFAULT_REDIRECT_URI_PORT = 12345;

namespace cloudstorage {
namespace {

struct HttpServerData {
  std::string code_;
  std::string code_parameter_name_;
  std::string error_parameter_name_;
  uint16_t port_;
  enum { Awaiting, Accepted, Denied } state_;
};

std::string sendHttpRequestFromJavaScript(const std::string& request) {
  return "<script>\n"
         "  var request = new XMLHttpRequest();\n"
         "  request.open(\"GET\", \"" +
         request +
         "\", false);\n"
         "  request.send(null);\n"
         "</script>\n";
}

int httpRequestCallback(void* cls, MHD_Connection* connection,
                        const char* /*url*/, const char* /*method*/,
                        const char* /*version*/, const char* /*upload_data*/,
                        size_t* /*upload_data_size*/, void** /*ptr*/) {
  HttpServerData* data = static_cast<HttpServerData*>(cls);
  std::string page;

  const char* code = MHD_lookup_connection_value(
      connection, MHD_GET_ARGUMENT_KIND, data->code_parameter_name_.c_str());
  if (code) {
    data->code_ = code;
    page = "<body>Success.</body>" +
           sendHttpRequestFromJavaScript("http://localhost:" +
                                         std::to_string(data->port_) +
                                         "/?accepted=true");
  }

  const char* error = MHD_lookup_connection_value(
      connection, MHD_GET_ARGUMENT_KIND, data->error_parameter_name_.c_str());
  if (error) {
    page = "<body>Error occurred.</body>" +
           sendHttpRequestFromJavaScript("http://localhost:" +
                                         std::to_string(data->port_) +
                                         "/?accepted=false");
  }

  const char* accepted = MHD_lookup_connection_value(
      connection, MHD_GET_ARGUMENT_KIND, "accepted");
  if (accepted) {
    if (std::string(accepted) == "true") {
      data->state_ = HttpServerData::Accepted;
    } else
      data->state_ = HttpServerData::Denied;
  }

  MHD_Response* response = MHD_create_response_from_buffer(
      page.length(), (void*)page.c_str(), MHD_RESPMEM_MUST_COPY);
  int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
  MHD_destroy_response(response);

  return ret;
}
}  // namespace

Auth::Auth() : redirect_uri_port_(DEFAULT_REDIRECT_URI_PORT) {}

bool Auth::authorize(ICallback* callback) {
  if (!access_token() || !validateToken(*access_token())) {
    Token::Pointer token;
    if (access_token() && (token = refreshToken())) {
      set_access_token(std::move(token));
    } else {
      if (callback) {
        if (callback->userConsentRequired(*this) == ICallback::Status::None)
          return false;
      }
      set_authorization_code(requestAuthorizationCode());
      set_access_token(requestAccessToken());
    }
  }
  return access_token() != nullptr;
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

std::string Auth::awaitAuthorizationCode(
    std::string code_parameter_name, std::string error_parameter_name) const {
  uint16_t http_server_port = redirect_uri_port();
  HttpServerData data = {"", code_parameter_name, error_parameter_name,
                         http_server_port, HttpServerData::Awaiting};
  MHD_Daemon* http_server =
      MHD_start_daemon(0, http_server_port, NULL, NULL, &httpRequestCallback,
                       &data, MHD_OPTION_END);
  fd_set rs, ws, es;
  MHD_socket max;
  while (data.state_ == HttpServerData::Awaiting) {
    FD_ZERO(&rs);
    FD_ZERO(&ws);
    FD_ZERO(&es);
    MHD_get_fdset(http_server, &rs, &ws, &es, &max);
    select(max + 1, &rs, &ws, &es, NULL);
    MHD_run_from_select(http_server, &rs, &ws, &es);
  }

  MHD_stop_daemon(http_server);

  if (data.state_ == HttpServerData::Accepted)
    return data.code_;
  else
    return "";
}

IAuth::Token::Pointer Auth::fromTokenString(
    const std::string& refresh_token) const {
  Token::Pointer token = make_unique<Token>();
  token->refresh_token_ = refresh_token;
  token->expires_in_ = -1;
  return token;
}

}  // namespace cloudstorage
