/*****************************************************************************
 * GoogleDrive.cpp : implementation of GoogleDrive
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

#include "GoogleDrive.h"

#include <jsoncpp/json/json.h>
#include <sstream>

#include "HttpRequest.h"
#include "Item.h"
#include "Utility.h"

namespace cloudstorage {

GoogleDrive::GoogleDrive() : CloudProvider(make_unique<Auth>()) {}

std::string GoogleDrive::name() const { return "google"; }

std::vector<IItem::Pointer> GoogleDrive::executeListDirectory(
    const IItem& f) const {
  const Item& item = static_cast<const Item&>(f);
  HttpRequest request("https://www.googleapis.com/drive/v3/files",
                      HttpRequest::Type::GET);
  request.setParameter("access_token", access_token());
  request.setParameter("q", std::string("'") + item.id() + "'+in+parents");

  std::vector<IItem::Pointer> result;
  while (true) {
    Json::Value response;
    std::stringstream stream(request.send());
    stream >> response;
    for (Json::Value v : response["files"]) {
      result.push_back(make_unique<Item>(
          v["name"].asString(), v["id"].asString(),
          v["mimeType"].asString() == "application/vnd.google-apps.folder"));
    }

    if (!response.isMember("nextPageToken"))
      break;
    else
      request.setParameter("pageToken", response["nextPageToken"].asString());
  }

  return result;
}

void GoogleDrive::executeUploadFile(const std::string&, std::istream&) const {
  // TODO
}

void GoogleDrive::executeDownloadFile(const IItem&, std::ostream&) const {
  // TODO
}

GoogleDrive::Auth::Auth() {
  set_client_id(
      "646432077068-hmvk44qgo6d0a64a5h9ieue34p3j2dcv.apps.googleusercontent."
      "com");
  set_client_secret("1f0FG5ch-kKOanTAv1Bqdp9U");
}

std::string GoogleDrive::Auth::authorizeLibraryUrl() const {
  std::string response_type = "code";
  std::string scope = "https://www.googleapis.com/auth/drive";
  std::string url = "https://accounts.google.com/o/oauth2/auth?";
  url += "response_type=" + response_type + "&";
  url += "client_id=" + client_id() + "&";
  url += "redirect_uri=" + redirect_uri() + "&";
  url += "scope=" + scope;
  return url;
}

std::string GoogleDrive::Auth::requestAuthorizationCode() const {
  return awaitAuthorizationCode("code", "error");
}

IAuth::Token::Pointer GoogleDrive::Auth::requestAccessToken() const {
  HttpRequest request("https://accounts.google.com/o/oauth2/token",
                      HttpRequest::Type::POST);
  request.setParameter("grant_type", "authorization_code");
  request.setParameter("client_secret", client_secret());
  request.setParameter("code", authorization_code());
  request.setParameter("client_id", client_id());
  request.setParameter("redirect_uri", redirect_uri());

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

IAuth::Token::Pointer GoogleDrive::Auth::refreshToken() const {
  if (!access_token()) return nullptr;

  HttpRequest request("https://accounts.google.com/o/oauth2/token",
                      HttpRequest::Type::POST);
  request.setParameter("refresh_token", access_token()->refresh_token_);
  request.setParameter("client_id", client_id());
  request.setParameter("client_secret", client_secret());
  request.setParameter("grant_type", "refresh_token");

  Json::Value response;
  std::stringstream(request.send()) >> response;

  if (response.isMember("access_token")) {
    Token::Pointer token = make_unique<Token>();
    token->token_ = response["access_token"].asString();
    token->refresh_token_ = access_token()->refresh_token_;
    token->expires_in_ = response["expires_in"].asInt();
    return token;
  }

  return nullptr;
}

bool GoogleDrive::Auth::validateToken(IAuth::Token& token) const {
  HttpRequest request("https://www.googleapis.com/oauth2/v3/tokeninfo",
                      HttpRequest::Type::GET);
  request.setParameter("access_token", token.token_);

  Json::Value response;
  std::stringstream(request.send()) >> response;

  if (response.isMember("expires_in")) {
    token.expires_in_ = atoi(response["expires_in"].asCString());
    return true;
  } else
    return false;
}

}  // namespace cloudstorage
