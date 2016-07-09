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
#include <iostream>
#include <sstream>

#include "Request/HttpCallback.h"
#include "Utility/HttpRequest.h"
#include "Utility/Item.h"
#include "Utility/Utility.h"

namespace cloudstorage {

OneDrive::OneDrive() : CloudProvider(make_unique<Auth>()) {}

std::string OneDrive::name() const { return "onedrive"; }

HttpRequest::Pointer OneDrive::getThumbnailRequest(const IItem& f,
                                                   std::ostream&) const {
  const Item& item = static_cast<const Item&>(f);
  return make_unique<HttpRequest>(item.thumbnail_url(), HttpRequest::Type::GET);
}

HttpRequest::Pointer OneDrive::listDirectoryRequest(
    const IItem& item, const std::string& page_token, std::ostream&) const {
  if (!page_token.empty())
    return make_unique<HttpRequest>(page_token, HttpRequest::Type::GET);
  HttpRequest::Pointer request = make_unique<HttpRequest>(
      "https://api.onedrive.com/v1.0/drive/items/" + item.id() + "/children",
      HttpRequest::Type::GET);
  request->setParameter(
      "select", "name,folder,audio,image,photo,video,id,@content.downloadUrl");

  return request;
}

HttpRequest::Pointer OneDrive::uploadFileRequest(
    const IItem& f, const std::string& filename, std::istream& stream,
    std::ostream& input_stream) const {
  const std::string separator = "1BPT6g1G6FUkXNkiqx5s";
  const Item& item = static_cast<const Item&>(f);
  HttpRequest::Pointer request = make_unique<HttpRequest>(
      "https://api.onedrive.com/v1.0/drive/items/" + item.id() + "/children",
      HttpRequest::Type::POST);
  request->setHeaderParameter(
      "Content-Type", "multipart/related; boundary=\"" + separator + "\"");
  Json::Value parameter;
  parameter["name"] = filename;
  parameter["file"] = {};
  parameter["@content.sourceUrl"] = "cid:content";
  input_stream << "--" << separator << "\r\n"
               << "Content-ID: <metadata>\r\n"
               << "Content-Type: application/json\r\n"
               << "\r\n"
               << parameter << "\r\n\r\n"
               << "--" << separator << "\r\n"
               << "Content-ID: <content>\r\n"
               << "Content-Type: application/octet-stream\r\n"
               << "\r\n"
               << stream.rdbuf() << "\r\n"
               << "--" << separator << "--\r\n";
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
    std::istream& stream, std::string& next_page_token) const {
  std::vector<IItem::Pointer> result;
  Json::Value response;
  stream >> response;
  for (Json::Value v : response["value"]) {
    IItem::FileType type = IItem::FileType::Unknown;
    if (v.isMember("folder"))
      type = IItem::FileType::Directory;
    else if (v.isMember("image") || v.isMember("photo"))
      type = IItem::FileType::Image;
    else if (v.isMember("video"))
      type = IItem::FileType::Video;
    else if (v.isMember("audio"))
      type = IItem::FileType::Audio;
    auto item =
        make_unique<Item>(v["name"].asString(), v["id"].asString(), type);
    item->set_url(v["@content.downloadUrl"].asString());
    item->set_thumbnail_url(
        "https://api.onedrive.com/v1.0/drive/items/" + item->id() +
        "/thumbnails/0/small/content?access_token=" + access_token());
    result.push_back(std::move(item));
  }
  if (response.isMember("@odata.nextLink"))
    next_page_token = response["@odata.nextLink"].asString();
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

HttpRequest::Pointer OneDrive::Auth::exchangeAuthorizationCodeRequest(
    std::ostream& data) const {
  HttpRequest::Pointer request = make_unique<HttpRequest>(
      "https://login.live.com/oauth20_token.srf", HttpRequest::Type::POST);
  data << "client_id=" << client_id() << "&"
       << "client_secret=" << client_secret() << "&"
       << "redirect_uri=" << redirect_uri() << "&"
       << "code=" << authorization_code() << "&"
       << "grant_type=authorization_code";
  return request;
}

HttpRequest::Pointer OneDrive::Auth::refreshTokenRequest(
    std::ostream& data) const {
  HttpRequest::Pointer request = make_unique<HttpRequest>(
      "https://login.live.com/oauth20_token.srf", HttpRequest::Type::POST);
  data << "client_id=" << client_id() << "&"
       << "client_secret=" << client_secret() << "&"
       << "refresh_token=" << access_token()->refresh_token_ << "&"
       << "grant_type=refresh_token";
  return request;
}

IAuth::Token::Pointer OneDrive::Auth::exchangeAuthorizationCodeResponse(
    std::istream& stream) const {
  Json::Value response;
  stream >> response;

  Token::Pointer token = make_unique<Token>();
  token->token_ = response["access_token"].asString();
  token->refresh_token_ = response["refresh_token"].asString();
  token->expires_in_ = response["expires_in"].asInt();
  return token;
}

IAuth::Token::Pointer OneDrive::Auth::refreshTokenResponse(
    std::istream& stream) const {
  Json::Value response;
  stream >> response;
  Token::Pointer token = make_unique<Token>();
  token->token_ = response["access_token"].asString();
  token->refresh_token_ = response["refresh_token"].asString();
  token->expires_in_ = response["expires_in"].asInt();
  return token;
}

}  // namespace cloudstorage
