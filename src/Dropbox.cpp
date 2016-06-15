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

std::vector<IItem::Pointer> Dropbox::executeListDirectory(
    const IItem& f) const {
  const Item& item = static_cast<const Item&>(f);
  HttpRequest request("https://api.dropboxapi.com/2/files/list_folder",
                      HttpRequest::Type::POST);
  request.setHeaderParameter("Authorization", "Bearer " + access_token());
  request.setHeaderParameter("Content-Type", "application/json");

  std::vector<IItem::Pointer> result;
  std::string cursor;
  while (true) {
    Json::Value parameter;
    if (!cursor.empty())
      parameter["cursor"] = cursor;
    else
      parameter["path"] = item.id();
    std::istringstream data_stream(parameter.toStyledString());
    Json::Value response;
    if (!Json::Reader().parse(request.send(data_stream), response)) {
      throw std::logic_error("This is not JSON!");
    }

    if (!response.isMember("entries"))
      throw std::logic_error("Invalid response.");
    for (Json::Value v : response["entries"]) {
      result.push_back(make_unique<Item>(v["name"].asString(),
                                         v["path_display"].asString(),
                                         v[".tag"].asString() == "folder"));
    }
    if (!response["has_more"].asBool()) break;

    request.set_url("https://api.dropboxapi.com/2/files/list_folder/continue");
    cursor = response["cursor"].asString();
  }
  return result;
}

std::string Dropbox::name() const { return "dropbox"; }

std::string Dropbox::token() const { return auth()->access_token()->token_; }

IItem::Pointer Dropbox::rootDirectory() const {
  return make_unique<Item>("/", "", true);
}

void Dropbox::executeUploadFile(const IItem& directory,
                                const std::string& filename,
                                std::istream& stream) const {
  const Item& item = static_cast<const Item&>(directory);
  HttpRequest request(
      "https://content.dropboxapi.com/1/files_put/auto/" + item.id() + filename,
      HttpRequest::Type::PUT);
  request.setHeaderParameter("Authorization", "Bearer " + access_token());
  Json::Value response;
  std::stringstream(request.send(stream)) >> response;
  if (response.isMember("error")) throw std::logic_error("Failed to upload.");
}

void Dropbox::executeDownloadFile(const IItem& f, std::ostream& stream) const {
  const Item& item = static_cast<const Item&>(f);
  HttpRequest request("https://content.dropboxapi.com/2/files/download",
                      HttpRequest::Type::POST);
  request.setHeaderParameter("Authorization",
                             std::string("Bearer ") + access_token());
  request.setHeaderParameter("Content-Type", "");
  Json::Value parameter;
  parameter["path"] = item.id();
  std::string str = Json::FastWriter().write(parameter);
  str.pop_back();
  request.setHeaderParameter("Dropbox-API-arg", str);
  request.send(stream);
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

std::string Dropbox::Auth::requestAuthorizationCode() const {
  return awaitAuthorizationCode("code", "error");
}

IAuth::Token::Pointer Dropbox::Auth::requestAccessToken() const {
  HttpRequest request("https://api.dropboxapi.com/oauth2/token",
                      HttpRequest::Type::POST);
  request.setParameter("grant_type", "authorization_code");
  request.setParameter("client_id", client_id());
  request.setParameter("client_secret", client_secret());
  request.setParameter("redirect_uri", redirect_uri());
  request.setParameter("code", authorization_code());

  Json::Value response;
  std::stringstream(request.send()) >> response;

  Token::Pointer token = make_unique<Token>();
  token->token_ = response["access_token"].asString();
  token->expires_in_ = -1;
  return token;
}

IAuth::Token::Pointer Dropbox::Auth::refreshToken() const {
  if (!access_token()) return nullptr;
  Token::Pointer token = make_unique<Token>(*access_token());
  if (!validateToken(*token)) return nullptr;
  return token;
}

bool Dropbox::Auth::validateToken(IAuth::Token& token) const {
  Dropbox dropbox;
  dropbox.auth()->set_access_token(make_unique<Token>(token));
  try {
    dropbox.executeListDirectory(*dropbox.rootDirectory());
  } catch (const std::exception&) {
    return false;
  }
  return true;
}

IAuth::Token::Pointer Dropbox::Auth::fromTokenString(
    const std::string& str) const {
  Token::Pointer token = make_unique<Token>();
  token->token_ = str;
  token->expires_in_ = -1;
  return token;
}

}  // namespace cloudstorage
