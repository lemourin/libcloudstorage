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

#include <json/json.h>
#include <sstream>

#include "Request/Request.h"
#include "Utility/HttpRequest.h"
#include "Utility/Item.h"
#include "Utility/Utility.h"

const uint32_t CHUNK_SIZE = 60 * 1024 * 1024;
using namespace std::placeholders;

namespace cloudstorage {

OneDrive::OneDrive() : CloudProvider(make_unique<Auth>()) {}

std::string OneDrive::name() const { return "onedrive"; }

ICloudProvider::UploadFileRequest::Pointer OneDrive::uploadFileAsync(
    IItem::Pointer parent, const std::string& filename,
    IUploadFileCallback::Pointer callback) {
  auto r = make_unique<Request<void>>(shared_from_this());
  r->set_error_callback([=](int code, const std::string& desc) {
    callback->error(std::to_string(code) + ": " + desc);
  });
  r->set_resolver([=](Request<void>* r) {
    std::stringstream output;
    int code = r->sendRequest(
        [=](std::ostream&) {
          return make_unique<HttpRequest>(
              "https://api.onedrive.com/v1.0/drive/items/" + parent->id() +
                  ":/" + HttpRequest::escape(filename) +
                  ":/upload.createSession",
              HttpRequest::Type::POST);
        },
        output);
    if (!HttpRequest::isSuccess(code)) {
      callback->error("Failed to create upload session.");
      return;
    }
    Json::Value response;
    output >> response;

    uint32_t sent = 0, size = callback->size();
    std::vector<char> buffer(CHUNK_SIZE);
    while (sent < size) {
      uint32_t length = callback->putData(buffer.begin().base(), CHUNK_SIZE);
      code = r->sendRequest(
          [=](std::ostream& stream) {
            auto request = make_unique<HttpRequest>(
                response["uploadUrl"].asString(), HttpRequest::Type::PUT);
            std::stringstream content_range;
            content_range << "bytes " << sent << "-" << sent + length - 1 << "/"
                          << size;
            request->setHeaderParameter("Content-Range", content_range.str());
            stream.write(buffer.data(), length);
            return request;
          },
          output, nullptr,
          [=](uint32_t, uint32_t now) {
            callback->progress(size, sent + now);
          });
      if (!HttpRequest::isSuccess(code)) {
        callback->error("Failed to upload chunk.");
        return;
      }
      sent += length;
    }
    callback->done();
  });
  return std::move(r);
}

HttpRequest::Pointer OneDrive::getItemDataRequest(const std::string& id,
                                                  std::ostream&) const {
  HttpRequest::Pointer request = make_unique<HttpRequest>(
      "https://api.onedrive.com/v1.0/drive/items/" + id,
      HttpRequest::Type::GET);
  request->setParameter(
      "select", "name,folder,audio,image,photo,video,id,@content.downloadUrl");
  return request;
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

HttpRequest::Pointer OneDrive::downloadFileRequest(const IItem& f,
                                                   std::ostream&) const {
  const Item& item = static_cast<const Item&>(f);
  HttpRequest::Pointer request = make_unique<HttpRequest>(
      "https://api.onedrive.com/v1.0/drive/items/" + item.id() + "/content",
      HttpRequest::Type::GET);
  return request;
}

HttpRequest::Pointer OneDrive::deleteItemRequest(const IItem& item,
                                                 std::ostream&) const {
  return make_unique<HttpRequest>(
      "https://api.onedrive.com/v1.0/drive/items/" + item.id(),
      HttpRequest::Type::DEL);
}

HttpRequest::Pointer OneDrive::createDirectoryRequest(
    const IItem& parent, const std::string& name, std::ostream& input) const {
  auto request = make_unique<HttpRequest>(
      "https://api.onedrive.com/v1.0/drive/items/" + parent.id() + "/children",
      HttpRequest::Type::POST);
  request->setHeaderParameter("Content-Type", "application/json");
  Json::Value json;
  json["name"] = name;
  json["folder"] = Json::Value(Json::objectValue);
  input << json;
  return std::move(request);
}

HttpRequest::Pointer OneDrive::moveItemRequest(const IItem& source,
                                               const IItem& destination,
                                               std::ostream& stream) const {
  auto request = make_unique<HttpRequest>(
      "https://api.onedrive.com/v1.0/drive/items/" + source.id(),
      HttpRequest::Type::PATCH);
  request->setHeaderParameter("Content-Type", "application/json");
  Json::Value json;
  if (destination.id() == rootDirectory()->id())
    json["parentReference"]["path"] = "/drive/root";
  else
    json["parentReference"]["id"] = destination.id();
  stream << json;
  return std::move(request);
}

HttpRequest::Pointer OneDrive::renameItemRequest(const IItem& item,
                                                 const std::string& name,
                                                 std::ostream& stream) const {
  auto request = make_unique<HttpRequest>(
      "https://api.onedrive.com/v1.0/drive/items/" + item.id(),
      HttpRequest::Type::PATCH);
  request->setHeaderParameter("Content-Type", "application/json");
  Json::Value json;
  json["name"] = name;
  stream << json;
  return std::move(request);
}

IItem::Pointer OneDrive::getItemDataResponse(std::istream& response) const {
  Json::Value json;
  response >> json;
  return toItem(json);
}

IItem::Pointer OneDrive::toItem(const Json::Value& v) const {
  IItem::FileType type = IItem::FileType::Unknown;
  if (v.isMember("folder"))
    type = IItem::FileType::Directory;
  else if (v.isMember("image") || v.isMember("photo"))
    type = IItem::FileType::Image;
  else if (v.isMember("video"))
    type = IItem::FileType::Video;
  else if (v.isMember("audio"))
    type = IItem::FileType::Audio;
  auto item = make_unique<Item>(v["name"].asString(), v["id"].asString(), type);
  item->set_url(v["@content.downloadUrl"].asString());
  item->set_thumbnail_url(
      "https://api.onedrive.com/v1.0/drive/items/" + item->id() +
      "/thumbnails/0/small/content?access_token=" + access_token());
  return std::move(item);
}

std::vector<IItem::Pointer> OneDrive::listDirectoryResponse(
    std::istream& stream, std::string& next_page_token) const {
  std::vector<IItem::Pointer> result;
  Json::Value response;
  stream >> response;
  for (Json::Value v : response["value"]) result.push_back(toItem(v));
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
