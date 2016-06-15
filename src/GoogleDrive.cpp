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
    if (!response.isMember("files"))
      throw std::logic_error("Invalid response.");
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

void GoogleDrive::executeUploadFile(const IItem& f, const std::string& filename,
                                    std::istream& stream) const {
  const Item& item = static_cast<const Item&>(f);
  const std::string separator = "fWoDm9QNn3v3Bq3bScUX";
  HttpRequest request(
      "https://www.googleapis.com/upload/drive/v3/files?uploadType=multipart",
      HttpRequest::Type::POST);
  request.setHeaderParameter("Authorization", "Bearer " + access_token());
  request.setHeaderParameter("Content-Type",
                             "multipart/related; boundary=" + separator);
  Json::Value request_data;
  request_data["name"] = filename;
  request_data["parents"].append(item.id());
  std::string json_data = Json::FastWriter().write(request_data);
  json_data.pop_back();
  std::stringstream data_stream;
  data_stream << "--" << separator << "\r\n"
              << "Content-Type: application/json; charset=UTF-8\r\n\r\n"
              << json_data << "\r\n"
              << "--" << separator << "\r\n"
              << "Content-Type: \r\n\r\n"
              << stream.rdbuf() << "\r\n"
              << "--" << separator << "--";
  std::stringstream response_stream;
  request.send(data_stream, response_stream);
  Json::Value response;
  response_stream >> response;
  if (!response.isMember("id"))
    throw std::logic_error("Failed to upload file.");
}

void GoogleDrive::executeDownloadFile(const IItem& f,
                                      std::ostream& stream) const {
  const Item& item = static_cast<const Item&>(f);
  HttpRequest request("https://www.googleapis.com/drive/v3/files/" + item.id(),
                      HttpRequest::Type::GET);
  request.setParameter("access_token", access_token());
  request.setParameter("alt", "media");
  request.send(stream);
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
  std::stringstream arguments;
  arguments << "grant_type=authorization_code&"
            << "client_secret=" << client_secret() << "&"
            << "code=" << authorization_code() << "&"
            << "client_id=" << client_id() << "&"
            << "redirect_uri=" << redirect_uri();

  Json::Value response;
  std::stringstream(request.send(static_cast<std::istream&>(arguments))) >>
      response;

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
  std::stringstream arguments;
  arguments << "refresh_token=" << access_token()->refresh_token_ << "&"
            << "client_id=" << client_id() << "&"
            << "client_secret=" << client_secret() << "&"
            << "redirect_uri=" << redirect_uri() << "&"
            << "grant_type=refresh_token";

  Json::Value response;
  std::stringstream(request.send(static_cast<std::istream&>(arguments))) >>
      response;

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
