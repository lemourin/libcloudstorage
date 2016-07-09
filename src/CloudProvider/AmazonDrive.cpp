/*****************************************************************************
 * AmazonDrive.cpp : AmazonDrive implementation
 *
 *****************************************************************************
 * Copyright (C) 2016-2016 VideoLAN
 *
 * Authors: Paweł Wegner <pawel.wegner95@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "AmazonDrive.h"

#include <jsoncpp/json/json.h>
#include <cstring>

#include "Request/HttpCallback.h"
#include "Utility/HttpRequest.h"
#include "Utility/Item.h"
#include "Utility/Utility.h"

namespace cloudstorage {

AmazonDrive::AmazonDrive() : CloudProvider(make_unique<Auth>()) {}

std::string AmazonDrive::name() const { return "amazon"; }

IItem::Pointer AmazonDrive::rootDirectory() const {
  std::unique_lock<std::mutex> lock(auth_mutex());
  if (root_id_.empty()) {
    lock.unlock();
    const_cast<AmazonDrive*>(this)->authorizeAsync()->finish();
    lock.lock();
  }
  return make_unique<Item>("/", root_id_, IItem::FileType::Directory);
}

cloudstorage::AuthorizeRequest::Pointer AmazonDrive::authorizeAsync() {
  return make_unique<AuthorizeRequest>(shared_from_this());
}

HttpRequest::Pointer AmazonDrive::listDirectoryRequest(
    const IItem& i, const std::string& page_token,
    std::ostream& input_stream) const {
  std::lock_guard<std::mutex> lock(auth_mutex());
  if (!page_token.empty()) {
    return make_unique<HttpRequest>(metadata_url_ + "nodes/" + i.id() +
                                        "/children?startToken=" + page_token,
                                    HttpRequest::Type::GET);
  }
  HttpRequest::Pointer request = make_unique<HttpRequest>(
      metadata_url_ + "nodes/" + i.id() + "/children?asset=ALL&tempLink=true",
      HttpRequest::Type::GET);
  return request;
}

HttpRequest::Pointer AmazonDrive::uploadFileRequest(
    const IItem& directory, const std::string& filename, std::istream& stream,
    std::ostream& input_stream) const {
  // TODO
  return nullptr;
}

HttpRequest::Pointer AmazonDrive::downloadFileRequest(const IItem& i,
                                                      std::ostream&) const {
  const Item& item = static_cast<const Item&>(i);
  return make_unique<HttpRequest>(item.url(), HttpRequest::Type::GET);
}

HttpRequest::Pointer AmazonDrive::getThumbnailRequest(const IItem& i,
                                                      std::ostream&) const {
  const Item& item = static_cast<const Item&>(i);
  HttpRequest::Pointer request =
      make_unique<HttpRequest>(item.thumbnail_url(), HttpRequest::Type::GET);
  return request;
}

std::vector<IItem::Pointer> AmazonDrive::listDirectoryResponse(
    std::istream& stream, std::string& next_page_token) const {
  Json::Value response;
  stream >> response;

  std::vector<IItem::Pointer> result;
  for (const Json::Value& v : response["data"]) {
    auto item = make_unique<Item>(v["name"].asString(), v["id"].asString(),
                                  kindToType(v["kind"].asString()));
    item->set_url(v["tempLink"].asString());
    result.push_back(std::move(item));
  }
  if (response.isMember("nextToken"))
    next_page_token = response["nextToken"].asString();

  return result;
}

IItem::FileType AmazonDrive::kindToType(const std::string& type) const {
  if (type == "FOLDER")
    return IItem::FileType::Directory;
  else
    return IItem::FileType::Unknown;
}

AmazonDrive::Auth::Auth() {
  set_client_id(
      "amzn1.application-oa2-client.04f642253f4e43668e5b1441ecf263f0");
  set_client_secret(
      "cd728b51d1668df8f33577a88e3ba531baa587bdbef2eb2d9b6ae89e95eaad22");
}

std::string AmazonDrive::Auth::authorizeLibraryUrl() const {
  std::string url =
      "https://www.amazon.com/ap/oa?client_id=" + client_id() +
      "&redirect_uri=" + redirect_uri() +
      "&response_type=code&scope=clouddrive:write+clouddrive:read_all";
  return url;
}

HttpRequest::Pointer AmazonDrive::Auth::exchangeAuthorizationCodeRequest(
    std::ostream& input_data) const {
  HttpRequest::Pointer request = make_unique<HttpRequest>(
      "https://api.amazon.com/auth/o2/token", HttpRequest::Type::POST);
  input_data << "grant_type=authorization_code&"
             << "code=" << authorization_code() << "&"
             << "client_id=" << client_id() << "&"
             << "client_secret=" << client_secret() << "&"
             << "redirect_uri=" << redirect_uri();
  return request;
}

HttpRequest::Pointer AmazonDrive::Auth::refreshTokenRequest(
    std::ostream& input_data) const {
  HttpRequest::Pointer request = make_unique<HttpRequest>(
      "https://api.amazon.com/auth/o2/token", HttpRequest::Type::POST);
  input_data << "grant_type=refresh_token&"
             << "refresh_token=" + access_token()->refresh_token_ << "&"
             << "client_id=" << client_id() << "&"
             << "client_secret=" << client_secret();
  return request;
}

IAuth::Token::Pointer AmazonDrive::Auth::exchangeAuthorizationCodeResponse(
    std::istream& stream) const {
  Json::Value response;
  stream >> response;
  auto token = make_unique<Token>();
  token->token_ = response["access_token"].asString();
  token->refresh_token_ = response["refresh_token"].asString();
  token->expires_in_ = response["expires_in"].asInt();
  return token;
}

IAuth::Token::Pointer AmazonDrive::Auth::refreshTokenResponse(
    std::istream& stream) const {
  Json::Value response;
  stream >> response;
  auto token = make_unique<Token>();
  token->refresh_token_ = access_token()->refresh_token_;
  token->token_ = response["access_token"].asString();
  token->expires_in_ = response["expires_in"].asInt();
  return token;
}

bool AmazonDrive::AuthorizeRequest::authorize() {
  if (!cloudstorage::AuthorizeRequest::authorize()) return false;
  HttpRequest request("https://drive.amazonaws.com/drive/v1/account/endpoint",
                      HttpRequest::Type::GET);
  request.setHeaderParameter(
      "Authorization", "Bearer " + provider()->auth()->access_token()->token_);
  std::stringstream input, output;
  int code = request.send(input, output, nullptr, httpCallback());
  if (!HttpRequest::isSuccess(code)) {
    provider()->callback()->error(*provider(), "Couldn't obtain endpoints.");
    return false;
  }
  Json::Value response;
  output >> response;
  auto drive = std::dynamic_pointer_cast<AmazonDrive>(provider());
  drive->metadata_url_ = response["metadataUrl"].asString();
  drive->content_url_ = response["contentUrl"].asString();

  input = std::stringstream();
  output = std::stringstream();
  request.set_url(drive->metadata_url_ + "/nodes?filters=isRoot:true");
  code = request.send(input, output, nullptr, httpCallback());
  if (!HttpRequest::isSuccess(code)) {
    provider()->callback()->error(*provider(),
                                  "Couldn't obtain root folder id.");
    return false;
  }
  output >> response;
  drive->root_id_ = response["data"][0]["id"].asString();
  return true;
}

}  // namespace cloudstorage
