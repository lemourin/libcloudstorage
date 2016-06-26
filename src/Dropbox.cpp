/*****************************************************************************
 * Dropbox.cpp : implementation of Dropbox
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

#include "Dropbox.h"

#include <jsoncpp/json/json.h>
#include <sstream>

#include "HttpRequest.h"
#include "Item.h"
#include "Utility.h"

namespace cloudstorage {

Dropbox::Dropbox() : CloudProvider(make_unique<Auth>()) {}

std::string Dropbox::name() const { return "dropbox"; }

IItem::Pointer Dropbox::rootDirectory() const {
  return make_unique<Item>("/", "", true);
}

HttpRequest::Pointer Dropbox::listDirectoryRequest(
    const IItem& f, std::ostream& input_stream) const {
  const Item& item = static_cast<const Item&>(f);
  HttpRequest::Pointer request =
      make_unique<HttpRequest>("https://api.dropboxapi.com/2/files/list_folder",
                               HttpRequest::Type::POST);
  request->setHeaderParameter("Content-Type", "application/json");

  Json::Value parameter;
  parameter["path"] = item.id();
  input_stream << Json::FastWriter().write(parameter);
  return request;
}

HttpRequest::Pointer Dropbox::uploadFileRequest(
    const IItem& directory, const std::string& filename, std::istream& stream,
    std::ostream& input_stream) const {
  const Item& item = static_cast<const Item&>(directory);
  HttpRequest::Pointer request = make_unique<HttpRequest>(
      "https://content.dropboxapi.com/1/files_put/auto/" + item.id() + filename,
      HttpRequest::Type::PUT);
  input_stream.rdbuf(stream.rdbuf());
  return request;
}

HttpRequest::Pointer Dropbox::downloadFileRequest(const IItem& f,
                                                  std::ostream&) const {
  const Item& item = static_cast<const Item&>(f);
  HttpRequest::Pointer request = make_unique<HttpRequest>(
      "https://content.dropboxapi.com/2/files/download",
      HttpRequest::Type::POST);
  request->setHeaderParameter("Content-Type", "");
  Json::Value parameter;
  parameter["path"] = item.id();
  std::string str = Json::FastWriter().write(parameter);
  str.pop_back();
  request->setHeaderParameter("Dropbox-API-arg", str);
  return request;
}

std::vector<IItem::Pointer> Dropbox::listDirectoryResponse(
    std::istream& stream, HttpRequest::Pointer& next_page_request,
    std::ostream& next_page_request_input) const {
  Json::Value response;
  stream >> response;

  std::vector<IItem::Pointer> result;
  for (Json::Value v : response["entries"]) {
    result.push_back(make_unique<Item>(v["name"].asString(),
                                       v["path_display"].asString(),
                                       v[".tag"].asString() == "folder"));
  }
  if (!response["has_more"].asBool())
    next_page_request = nullptr;
  else {
    next_page_request->set_url(
        "https://api.dropboxapi.com/2/files/list_folder/continue");
    Json::Value input;
    input["cursor"] = response["cursor"];
    next_page_request_input << input;
  }
  return result;
}

Dropbox::Auth::Auth() {
  set_client_id("ktryxp68ae5cicj");
  set_client_secret("6evu94gcxnmyr59");
}

std::string Dropbox::Auth::authorizeLibraryUrl() const {
  std::string url = "https://www.dropbox.com/oauth2/authorize?";
  url += "response_type=code&";
  url += "client_id=" + client_id() + "&";
  url += "redirect_uri=" + redirect_uri() + "&";
  return url;
}

IAuth::Token::Pointer Dropbox::Auth::fromTokenString(
    const std::string& str) const {
  Token::Pointer token = make_unique<Token>();
  token->token_ = str;
  token->refresh_token_ = str;
  token->expires_in_ = -1;
  return token;
}

HttpRequest::Pointer Dropbox::Auth::exchangeAuthorizationCodeRequest(
    std::ostream&) const {
  HttpRequest::Pointer request = make_unique<HttpRequest>(
      "https://api.dropboxapi.com/oauth2/token", HttpRequest::Type::POST);
  request->setParameter("grant_type", "authorization_code");
  request->setParameter("client_id", client_id());
  request->setParameter("client_secret", client_secret());
  request->setParameter("redirect_uri", redirect_uri());
  request->setParameter("code", authorization_code());
  return request;
}

HttpRequest::Pointer Dropbox::Auth::refreshTokenRequest(
    std::ostream& input_data) const {
  return validateTokenRequest(input_data);
}

HttpRequest::Pointer Dropbox::Auth::validateTokenRequest(
    std::ostream& stream) const {
  Dropbox dropbox;
  dropbox.auth()->set_access_token(make_unique<Token>(*access_token()));

  HttpRequest::Pointer r = dropbox.listDirectoryRequest(
      *dropbox.rootDirectory(), static_cast<std::ostream&>(stream));
  dropbox.authorizeRequest(*r);
  return r;
}

IAuth::Token::Pointer Dropbox::Auth::exchangeAuthorizationCodeResponse(
    std::istream& stream) const {
  Json::Value response;
  stream >> response;

  Token::Pointer token = make_unique<Token>();
  token->token_ = response["access_token"].asString();
  token->refresh_token_ = token->token_;
  token->expires_in_ = -1;
  return token;
}

IAuth::Token::Pointer Dropbox::Auth::refreshTokenResponse(std::istream&) const {
  return make_unique<Token>(*access_token());
}

bool Dropbox::Auth::validateTokenResponse(std::istream&) const { return true; }

}  // namespace cloudstorage
