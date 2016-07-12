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

#include <jsoncpp/json/json.h>

#include "Request/Request.h"
#include "Utility/Item.h"
#include "Utility/Utility.h"

namespace cloudstorage {

Box::Box() : CloudProvider(make_unique<Auth>()) {}

IItem::Pointer Box::rootDirectory() const {
  return make_unique<Item>("root", "0", IItem::FileType::Directory);
}

std::string Box::name() const { return "box"; }

bool Box::reauthorize(int code) const {
  return HttpRequest::isClientError(code);
}

ICloudProvider::GetItemDataRequest::Pointer Box::getItemDataAsync(
    const std::string& id, GetItemDataCallback callback) {
  auto r = make_unique<Request<IItem::Pointer>>(shared_from_this());
  r->set_resolver([id, callback,
                   this](Request<IItem::Pointer>* r) -> IItem::Pointer {
    std::stringstream output;
    int code = r->sendRequest(
        [id](std::ostream&) {
          return make_unique<HttpRequest>("https://api.box.com/2.0/files/" + id,
                                          HttpRequest::Type::GET);
        },
        output);
    if (!HttpRequest::isSuccess(code)) {
      callback(nullptr);
      return nullptr;
    }
    Json::Value response;
    output >> response;
    auto item = toItem(response);
    code = r->sendRequest(
        [id](std::ostream&) {
          auto request = make_unique<HttpRequest>(
              "https://api.box.com/2.0/files/" + id + "/content",
              HttpRequest::Type::GET);
          request->set_follow_redirect(false);
          return request;
        },
        output);
    if (HttpRequest::isSuccess(code)) {
      std::string redirect_url;
      output >> redirect_url;
      static_cast<Item*>(item.get())->set_url(redirect_url);
    }
    callback(item);
    return item;
  });
  return r;
}

HttpRequest::Pointer Box::getItemDataRequest(const std::string&,
                                             std::ostream&) const {
  return nullptr;
}

HttpRequest::Pointer Box::listDirectoryRequest(const IItem& item,
                                               const std::string& page_token,
                                               std::ostream&) const {
  auto request = make_unique<HttpRequest>(
      "https://api.box.com/2.0/folders/" + item.id() + "/items/",
      HttpRequest::Type::GET);
  if (!page_token.empty()) request->setParameter("offset", page_token);
  return request;
}

HttpRequest::Pointer Box::uploadFileRequest(const IItem& directory,
                                            const std::string& filename,
                                            std::istream& stream,
                                            std::ostream& input_stream) const {
  const std::string separator = "Thnlg1ecwyUJHyhYYGrQ";
  HttpRequest::Pointer request = make_unique<HttpRequest>(
      "https://upload.box.com/api/2.0/files/content", HttpRequest::Type::POST);
  request->setHeaderParameter("Content-Type",
                              "multipart/form-data; boundary=" + separator);
  Json::Value json;
  json["name"] = filename;
  Json::Value parent;
  parent["id"] = directory.id();
  json["parent"] = parent;
  std::string json_data = Json::FastWriter().write(json);
  json_data.pop_back();
  input_stream << "--" << separator << "\r\n"
               << "Content-Disposition: form-data; name=\"attributes\"\r\n\r\n"
               << json_data << "\r\n"
               << "--" << separator << "\r\n"
               << "Content-Disposition: form-data; name=\"file\"; filename=\""
               << filename << "\"\r\n"
               << "Content-Type: application/octet-stream\r\n\r\n"
               << stream.rdbuf() << "\r\n"
               << "\r\n"
               << "--" << separator << "--\r\n";
  return request;
}

HttpRequest::Pointer Box::downloadFileRequest(const IItem& item,
                                              std::ostream&) const {
  return make_unique<HttpRequest>(
      "https://api.box.com/2.0/files/" + item.id() + "/content",
      HttpRequest::Type::GET);
}

HttpRequest::Pointer Box::getThumbnailRequest(const IItem& item,
                                              std::ostream&) const {
  return make_unique<HttpRequest>(
      "https://api.box.com/2.0/files/" + item.id() + "/thumbnail.png",
      HttpRequest::Type::GET);
}

IItem::Pointer Box::getItemDataResponse(std::istream& stream) const {
  Json::Value response;
  stream >> response;
  auto item = toItem(response);
  return item;
}

std::vector<IItem::Pointer> Box::listDirectoryResponse(
    std::istream& stream, std::string& next_page_token) const {
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
  auto item = make_unique<Item>(v["name"].asString(), v["id"].asString(), type);
  return item;
}

Box::Auth::Auth() {
  set_client_id("zmiv9tv13hunxhyjk16zqv8dmdw0d773");
  set_client_secret("IZ0T8WsUpJin7Qt3rHMf7qDAIFAkYZ0R");
}

std::string Box::Auth::authorizeLibraryUrl() const {
  return "https://account.box.com/api/oauth2/"
         "authorize?response_type=code&client_id=" +
         client_id() + "&redirect_uri=" + redirect_uri() + "&state=whatever";
}

HttpRequest::Pointer Box::Auth::exchangeAuthorizationCodeRequest(
    std::ostream& input_data) const {
  auto request = make_unique<HttpRequest>("https://api.box.com/oauth2/token",
                                          HttpRequest::Type::POST);
  input_data << "grant_type=authorization_code&"
             << "code=" << authorization_code() << "&"
             << "client_id=" << client_id() << "&"
             << "client_secret=" << client_secret();
  return request;
}

HttpRequest::Pointer Box::Auth::refreshTokenRequest(
    std::ostream& input_data) const {
  auto request = make_unique<HttpRequest>("https://api.box.com/oauth2/token",
                                          HttpRequest::Type::POST);
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
  return make_unique<Token>(Token{response["access_token"].asString(),
                                  response["refresh_token"].asString(), -1});
}

IAuth::Token::Pointer Box::Auth::refreshTokenResponse(
    std::istream& stream) const {
  Json::Value response;
  stream >> response;
  return make_unique<Token>(Token{response["access_token"].asString(),
                                  access_token()->refresh_token_, -1});
}

}  // namespace cloudstorage
