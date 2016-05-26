/*****************************************************************************
 * OneDrive.cpp : implementation of OneDrive
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

#include "OneDrive.h"

#include <jsoncpp/json/json.h>
#include <sstream>

#include "HttpRequest.h"
#include "Item.h"
#include "Utility.h"

namespace cloudstorage {

OneDrive::OneDrive() : CloudProvider(make_unique<Auth>()) {}

std::string OneDrive::name() const { return "onedrive"; }

std::vector<IItem::Pointer> OneDrive::executeListDirectory(
    const IItem& f) const {
  const Item& item = static_cast<const Item&>(f);
  HttpRequest request(
      std::string("https://api.onedrive.com/v1.0/drive/items/") + item.id() +
          "/children",
      HttpRequest::Type::GET);
  request.setParameter("access_token", access_token());
  request.setParameter("select", "name,folder,id");

  std::vector<IItem::Pointer> result;
  while (true) {
    Json::Value response;
    std::stringstream(request.send()) >> response;
    for (Json::Value v : response["value"]) {
      result.push_back(make_unique<Item>(
          v["name"].asString(), v["id"].asString(), v.isMember("folder")));
    }

    if (!response.isMember("@odata.nextLink")) break;
    request.reset_parameters();
    request.set_url(response["@odata.nextLink"].asString());
  }
  return result;
}

void OneDrive::executeUploadFile(const std::string&, std::istream&) const {
  // TODO
}

void OneDrive::executeDownloadFile(const IItem&, std::ostream&) const {
  // TODO
}

OneDrive::Auth::Auth() {
  set_client_id("56a1d60f-ea71-40e9-a489-b87fba12a23e");
  set_client_secret("zJRAsd0o4E9c33q4OLc7OhY");
}

std::string OneDrive::Auth::authorizeLibraryUrl() const {
  std::string result =
      std::string("https://login.live.com/oauth20_authorize.srf?");
  std::string scope = "wl.signin%20wl.offline_access%20onedrive.readwrite";
  std::string response_type = "code";
  result += "client_id=" + client_id() + "&";
  result += "scope=" + scope + "&";
  result += "response_type=" + response_type + "&";
  result += "redirect_uri=" + redirect_uri() + "&";
  return result;
}

std::string OneDrive::Auth::requestAuthorizationCode() const {
  return awaitAuthorizationCode("code", "error");
}

IAuth::Token::Pointer OneDrive::Auth::requestAccessToken() const {
  HttpRequest request("https://login.live.com/oauth20_token.srf",
                      HttpRequest::Type::POST);
  request.setParameter("client_id", client_id());
  request.setParameter("client_secret", client_secret());
  request.setParameter("redirect_uri", redirect_uri());
  request.setParameter("code", authorization_code());
  request.setParameter("grant_type", "authorization_code");

  Json::Value response;
  std::stringstream(request.send()) >> response;

  Token::Pointer token = make_unique<Token>();
  token->token_ = response["access_token"].asString();
  token->refresh_token_ = response["refresh_token"].asString();
  token->expires_in_ = response["expires_in"].asInt();

  return token;
}

IAuth::Token::Pointer OneDrive::Auth::refreshToken() const {
  if (!access_token()) return nullptr;
  HttpRequest request("https://login.live.com/oauth20_token.srf",
                      HttpRequest::Type::POST);
  request.setParameter("client_id", client_id());
  request.setParameter("client_secret", client_secret());
  request.setParameter("refresh_token", access_token()->refresh_token_);
  request.setParameter("grant_type", "refresh_token");
  Json::Value response;
  std::stringstream(request.send()) >> response;
  if (response.isMember("access_token")) {
    Token::Pointer token = make_unique<Token>();
    token->token_ = response["access_token"].asString();
    token->refresh_token_ = response["refresh_token"].asString();
    token->expires_in_ = response["expires_in"].asInt();
    return token;
  }
  return nullptr;
}

bool OneDrive::Auth::validateToken(IAuth::Token& token) const {
  Token::Pointer t = refreshToken();
  if (!t) return false;
  token = *t;
  return true;
}

}  // namespace cloudstorage
