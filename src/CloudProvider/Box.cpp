/*****************************************************************************
 * Box.cpp : Box implementation
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

#include "Box.h"

#include <json/json.h>

#include "Request/Request.h"
#include "Utility/Item.h"
#include "Utility/Utility.h"

const std::string BOXAPI_ENDPOINT = "https://api.box.com";

namespace cloudstorage {

using util::FileId;

Box::Box() : CloudProvider(util::make_unique<Auth>()) {}

IItem::Pointer Box::rootDirectory() const {
  return util::make_unique<Item>("root", FileId(true, "0"), IItem::UnknownSize,
                                 IItem::UnknownTimeStamp,
                                 IItem::FileType::Directory);
}

std::string Box::name() const { return "box"; }

std::string Box::endpoint() const { return BOXAPI_ENDPOINT; }

bool Box::reauthorize(int code, const IHttpRequest::HeaderParameters&) const {
  return IHttpRequest::isClientError(code) && code != IHttpRequest::NotFound;
}

IHttpRequest::Pointer Box::getItemUrlRequest(const IItem& item,
                                             std::ostream&) const {
  auto request = http()->create(
      endpoint() + "/2.0/files/" + FileId(item.id()).id_ + "/content", "GET",
      false);
  return request;
}

std::string Box::getItemUrlResponse(
    const IItem&, const IHttpRequest::HeaderParameters& header,
    std::istream&) const {
  auto it = header.find("location");
  if (it == header.end()) throw std::logic_error("no location header");
  return it->second;
}

IHttpRequest::Pointer Box::getItemDataRequest(const std::string& id,
                                              std::ostream&) const {
  auto data = FileId(id);
  if (data.folder_)
    return http()->create(endpoint() + "/2.0/folders/" + data.id_, "GET");
  else
    return http()->create(endpoint() + "/2.0/files/" + data.id_, "GET");
}

IHttpRequest::Pointer Box::listDirectoryRequest(const IItem& item,
                                                const std::string& page_token,
                                                std::ostream&) const {
  auto request = http()->create(
      endpoint() + "/2.0/folders/" + FileId(item.id()).id_ + "/items/", "GET");
  request->setParameter("fields", "name,id,size,modified_at");
  if (!page_token.empty()) request->setParameter("offset", page_token);
  return request;
}

IHttpRequest::Pointer Box::uploadFileRequest(
    const IItem& directory, const std::string& filename,
    std::ostream& prefix_stream, std::ostream& suffix_stream) const {
  const std::string separator = "Thnlg1ecwyUJHyhYYGrQ";
  IHttpRequest::Pointer request =
      http()->create("https://upload.box.com/api/2.0/files/content", "POST");
  request->setHeaderParameter("Content-Type",
                              "multipart/form-data; boundary=" + separator);
  Json::Value json;
  json["name"] = filename;
  Json::Value parent;
  parent["id"] = FileId(directory.id()).id_;
  json["parent"] = parent;
  prefix_stream << "--" << separator << "\r\n"
                << "Content-Disposition: form-data; name=\"attributes\"\r\n\r\n"
                << util::to_string(json) << "\r\n"
                << "--" << separator << "\r\n"
                << "Content-Disposition: form-data; name=\"file\"; filename=\""
                << util::Url::escapeHeader(filename) << "\"\r\n"
                << "Content-Type: application/octet-stream\r\n\r\n";
  suffix_stream << "\r\n--" << separator << "--";
  return request;
}

IItem::Pointer Box::uploadFileResponse(const IItem&, const std::string&,
                                       uint64_t, std::istream& response) const {
  Json::Value json;
  response >> json;
  return toItem(json["entries"][0]);
}

IHttpRequest::Pointer Box::downloadFileRequest(const IItem& item,
                                               std::ostream&) const {
  return http()->create(
      endpoint() + "/2.0/files/" + FileId(item.id()).id_ + "/content", "GET");
}

IHttpRequest::Pointer Box::getThumbnailRequest(const IItem& item,
                                               std::ostream&) const {
  return http()->create(
      endpoint() + "/2.0/files/" + FileId(item.id()).id_ + "/thumbnail.png",
      "GET");
}

IHttpRequest::Pointer Box::deleteItemRequest(const IItem& item,
                                             std::ostream&) const {
  auto data = FileId(item.id());
  if (item.type() == IItem::FileType::Directory) {
    auto r = http()->create(endpoint() + "/2.0/folders/" + data.id_, "DELETE");
    r->setParameter("recursive", "true");
    return r;
  } else
    return http()->create(endpoint() + "/2.0/files/" + data.id_, "DELETE");
}

IHttpRequest::Pointer Box::createDirectoryRequest(const IItem& item,
                                                  const std::string& name,
                                                  std::ostream& stream) const {
  auto request = http()->create(endpoint() + "/2.0/folders", "POST");
  request->setHeaderParameter("Content-Type", "application/json");
  Json::Value json;
  json["name"] = name;
  json["parent"]["id"] = FileId(item.id()).id_;
  stream << json;
  return request;
}

IHttpRequest::Pointer Box::moveItemRequest(const IItem& source,
                                           const IItem& destination,
                                           std::ostream& stream) const {
  IHttpRequest::Pointer request;
  auto data = FileId(source.id());
  if (source.type() == IItem::FileType::Directory)
    request = http()->create(endpoint() + "/2.0/folders/" + data.id_, "PUT");
  else
    request = http()->create(endpoint() + "/2.0/files/" + data.id_, "PUT");

  request->setHeaderParameter("Content-Type", "application/json");
  Json::Value json;
  json["parent"]["id"] = FileId(destination.id()).id_;
  stream << json;
  return request;
}

IHttpRequest::Pointer Box::renameItemRequest(const IItem& item,
                                             const std::string& name,
                                             std::ostream& input) const {
  IHttpRequest::Pointer request;
  auto data = FileId(item.id());
  if (item.type() == IItem::FileType::Directory)
    request = http()->create(endpoint() + "/2.0/folders/" + data.id_, "PUT");
  else
    request = http()->create(endpoint() + "/2.0/files/" + data.id_, "PUT");
  Json::Value json;
  json["name"] = name;
  input << json;
  return request;
}

IItem::Pointer Box::getItemDataResponse(std::istream& stream) const {
  Json::Value response;
  stream >> response;
  auto item = toItem(response);
  return item;
}

std::vector<IItem::Pointer> Box::listDirectoryResponse(
    const IItem&, std::istream& stream, std::string& next_page_token) const {
  Json::Value response;
  stream >> response;
  std::vector<IItem::Pointer> result;
  for (const Json::Value& v : response["entries"]) result.push_back(toItem(v));
  int offset = response["offset"].asInt();
  int limit = response["limit"].asInt();
  int total_count = response["total_count"].asInt();
  if (offset + limit < total_count)
    next_page_token = std::to_string(offset + limit);
  return result;
}

IItem::Pointer Box::toItem(const Json::Value& v) const {
  IItem::FileType type = IItem::FileType::Unknown;
  if (v["type"].asString() == "folder") type = IItem::FileType::Directory;
  auto item = util::make_unique<Item>(
      v["name"].asString(),
      FileId(type == IItem::FileType::Directory, v["id"].asString()),
      v["size"].asUInt64(), util::parse_time(v["modified_at"].asString()),
      type);
  return std::move(item);
}

void Box::Auth::initialize(IHttp* http, IHttpServerFactory* factory) {
  cloudstorage::Auth::initialize(http, factory);
  if (client_id().empty()) {
    set_client_id("zmiv9tv13hunxhyjk16zqv8dmdw0d773");
    set_client_secret("IZ0T8WsUpJin7Qt3rHMf7qDAIFAkYZ0R");
  }
}

std::string Box::Auth::authorizeLibraryUrl() const {
  return "https://account.box.com/api/oauth2/"
         "authorize?response_type=code&client_id=" +
         client_id() + "&redirect_uri=" + redirect_uri() + "&state=" + state();
}

IHttpRequest::Pointer Box::Auth::exchangeAuthorizationCodeRequest(
    std::ostream& input_data) const {
  auto request = http()->create(BOXAPI_ENDPOINT + "/oauth2/token", "POST");
  input_data << "grant_type=authorization_code&"
             << "code=" << authorization_code() << "&"
             << "client_id=" << client_id() << "&"
             << "client_secret=" << client_secret();
  return request;
}

IHttpRequest::Pointer Box::Auth::refreshTokenRequest(
    std::ostream& input_data) const {
  auto request = http()->create(BOXAPI_ENDPOINT + "/oauth2/token", "POST");
  input_data << "grant_type=refresh_token&"
             << "refresh_token=" << access_token()->refresh_token_ << "&"
             << "client_id=" << client_id() << "&"
             << "client_secret=" << client_secret() << "&"
             << "redirect_uri=" << redirect_uri();
  return request;
}

IAuth::Token::Pointer Box::Auth::exchangeAuthorizationCodeResponse(
    std::istream& stream) const {
  Json::Value response;
  stream >> response;
  return util::make_unique<Token>(Token{response["access_token"].asString(),
                                        response["refresh_token"].asString(),
                                        -1});
}

IAuth::Token::Pointer Box::Auth::refreshTokenResponse(
    std::istream& stream) const {
  Json::Value response;
  stream >> response;
  return util::make_unique<Token>(Token{response["access_token"].asString(),
                                        response["refresh_token"].asString(),
                                        -1});
}

}  // namespace cloudstorage
