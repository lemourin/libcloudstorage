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

std::vector<IItem::Pointer> OneDrive::executeListDirectory(const IItem& f) {
  const Item& item = static_cast<const Item&>(f);
  HttpRequest request(
      "https://api.onedrive.com/v1.0/drive/items/" + item.id() + "/children",
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
    request.resetParameters();
    request.set_url(response["@odata.nextLink"].asString());
  }
  return result;
}

void OneDrive::executeUploadFile(const IItem& f, const std::string& filename,
                                 std::istream& stream) {
  const Item& item = static_cast<const Item&>(f);
  HttpRequest request("https://api.onedrive.com/v1.0/drive/items/" + item.id() +
                          ":/" + filename + ":/content",
                      HttpRequest::Type::PUT);
  request.setHeaderParameter("Authorization", "Bearer " + access_token());
  Json::Value response;
  std::stringstream(request.send(stream)) >> response;
  if (response.isMember("error"))
    throw std::logic_error("Failed to upload file.");
}

void OneDrive::executeDownloadFile(const IItem& f, std::ostream& stream) {
  const Item& item = static_cast<const Item&>(f);
  HttpRequest request(
      "https://api.onedrive.com/v1.0/drive/items/" + item.id() + "/content",
      HttpRequest::Type::GET);
  request.setHeaderParameter("Authorization", "Bearer " + access_token());
  request.send(stream);
}

HttpRequest::Pointer OneDrive::listDirectoryRequest(const IItem& f,
                                                    std::ostream&) const {
  const Item& item = static_cast<const Item&>(f);
  HttpRequest::Pointer request = make_unique<HttpRequest>(
      "https://api.onedrive.com/v1.0/drive/items/" + item.id() + "/children",
      HttpRequest::Type::GET);
  request->setParameter("select", "name,folder,id");
  return request;
}

HttpRequest::Pointer OneDrive::uploadFileRequest(
    const IItem& f, const std::string& filename, std::istream& stream,
    std::ostream& input_stream) const {
  const Item& item = static_cast<const Item&>(f);
  HttpRequest::Pointer request =
      make_unique<HttpRequest>("https://api.onedrive.com/v1.0/drive/items/" +
                                   item.id() + ":/" + filename + ":/content",
                               HttpRequest::Type::PUT);
  input_stream.rdbuf(stream.rdbuf());
  return request;
}

HttpRequest::Pointer OneDrive::downloadFileRequest(const IItem& f,
                                                   std::ostream&) const {
  const Item& item = static_cast<const Item&>(f);
  HttpRequest::Pointer request = make_unique<HttpRequest>(
      "https://api.onedrive.com/v1.0/drive/items/" + item.id() + "/content",
      HttpRequest::Type::GET);
  return request;
}

std::vector<IItem::Pointer> OneDrive::listDirectoryResponse(
    std::istream& stream, HttpRequest::Pointer& next_page_request) const {
  std::vector<IItem::Pointer> result;
  Json::Value response;
  stream >> response;
  for (Json::Value v : response["value"]) {
    result.push_back(make_unique<Item>(v["name"].asString(), v["id"].asString(),
                                       v.isMember("folder")));
  }
  if (response.isMember("@odata.nextLink")) {
    next_page_request->resetParameters();
    next_page_request->set_url(response["@odata.nextLink"].asString());
  } else
    next_page_request = nullptr;
  return result;
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
  std::stringstream data;
  data << "client_id=" << client_id() << "&"
       << "client_secret=" << client_secret() << "&"
       << "redirect_uri=" << redirect_uri() << "&"
       << "code=" << authorization_code() << "&"
       << "grant_type=authorization_code";

  Json::Value response;
  std::stringstream(request.send(static_cast<std::istream&>(data))) >> response;

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
  std::stringstream data;
  data << "client_id=" << client_id() << "&"
       << "client_secret=" << client_secret() << "&"
       << "refresh_token=" << access_token()->refresh_token_ << "&"
       << "grant_type=refresh_token";
  Json::Value response;
  std::stringstream(request.send(static_cast<std::istream&>(data))) >> response;
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
