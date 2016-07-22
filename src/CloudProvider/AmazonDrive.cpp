/*****************************************************************************
 * AmazonDrive.cpp : AmazonDrive implementation
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

#include "AmazonDrive.h"

#include <json/json.h>

#include "Utility/HttpRequest.h"
#include "Utility/Item.h"
#include "Utility/Utility.h"

const int THUMBNAIL_SIZE = 64;

namespace cloudstorage {

AmazonDrive::AmazonDrive() : CloudProvider(make_unique<Auth>()) {}

void AmazonDrive::initialize(const std::string& token,
                             ICloudProvider::ICallback::Pointer callback,
                             const ICloudProvider::Hints& hints) {
  CloudProvider::initialize(token, std::move(callback), hints);
  std::unique_lock<std::mutex> lock(auth_mutex());
  setWithHint(hints, "metadata_url",
              [this](std::string v) { metadata_url_ = v; });
  setWithHint(hints, "content_url",
              [this](std::string v) { content_url_ = v; });
}

ICloudProvider::Hints AmazonDrive::hints() const {
  Hints result = {{"metadata_url", metadata_url()},
                  {"content_url", content_url()}};
  auto t = CloudProvider::hints();
  result.insert(t.begin(), t.end());
  return result;
}

std::string AmazonDrive::name() const { return "amazon"; }

IItem::Pointer AmazonDrive::rootDirectory() const {
  return make_unique<Item>("root", "root", IItem::FileType::Directory);
}

ICloudProvider::MoveItemRequest::Pointer AmazonDrive::moveItemAsync(
    IItem::Pointer s, IItem::Pointer d, MoveItemCallback callback) {
  auto r = make_unique<Request<bool>>(shared_from_this());
  r->set_resolver([=](Request<bool>* r) -> bool {
    Item* source = static_cast<Item*>(s.get());
    Item* destination = static_cast<Item*>(d.get());
    for (const std::string& parent : source->parents()) {
      std::stringstream output;
      int code = r->sendRequest(
          [=](std::ostream& stream) {
            auto request = make_unique<HttpRequest>(
                metadata_url() + "/nodes/" + destination->id() + "/children",
                HttpRequest::Type::POST);
            request->setHeaderParameter("Content-Type", "application/json");
            Json::Value json;
            json["fromParent"] = parent;
            json["childId"] = source->id();
            stream << json;
            return request;
          },
          output);
      if (!HttpRequest::isSuccess(code)) {
        callback(false);
        return false;
      }
    }
    callback(true);
    return true;
  });
  return r;
}

AuthorizeRequest::Pointer AmazonDrive::authorizeAsync() {
  auto r = std::make_shared<AuthorizeRequest>(
      shared_from_this(), [this](AuthorizeRequest* r) -> bool {
        if (!r->oauth2Authorization()) return false;
        HttpRequest request(
            "https://drive.amazonaws.com/drive/v1/account/endpoint",
            HttpRequest::Type::GET);
        authorizeRequest(request);
        std::stringstream input, output;
        int code = r->send(&request, input, output, nullptr);
        if (!HttpRequest::isSuccess(code)) {
          if (!r->is_cancelled())
            callback()->error(*this, "Couldn't obtain endpoints.");
          return false;
        }
        Json::Value response;
        output >> response;
        {
          std::unique_lock<std::mutex> lock(auth_mutex());
          metadata_url_ = response["metadataUrl"].asString();
          content_url_ = response["contentUrl"].asString();
        }
        return true;
      });
  return r;
}

HttpRequest::Pointer AmazonDrive::getItemDataRequest(const std::string& id,
                                                     std::ostream&) const {
  auto request = make_unique<HttpRequest>(metadata_url() + "/nodes/" + id,
                                          HttpRequest::Type::GET);
  request->setParameter("tempLink", "true");
  return request;
}

HttpRequest::Pointer AmazonDrive::listDirectoryRequest(
    const IItem& i, const std::string& page_token, std::ostream&) const {
  if (i.id() == rootDirectory()->id()) {
    return make_unique<HttpRequest>(
        metadata_url() + "/nodes?filters=isRoot:true", HttpRequest::Type::GET);
  }
  if (!page_token.empty()) {
    return make_unique<HttpRequest>(metadata_url() + "nodes/" + i.id() +
                                        "/children?startToken=" + page_token,
                                    HttpRequest::Type::GET);
  }
  HttpRequest::Pointer request = make_unique<HttpRequest>(
      metadata_url() + "nodes/" + i.id() + "/children?asset=ALL&tempLink=true",
      HttpRequest::Type::GET);
  return request;
}

HttpRequest::Pointer AmazonDrive::uploadFileRequest(
    const IItem& directory, const std::string& filename, std::istream& stream,
    std::ostream& input_stream) const {
  if (directory.id() == rootDirectory()->id()) return nullptr;
  const std::string separator = "Thnlg1ecwyUJHyhYYGrQ";
  HttpRequest::Pointer request = make_unique<HttpRequest>(
      content_url() + "/nodes", HttpRequest::Type::POST);
  request->setHeaderParameter("Content-Type",
                              "multipart/form-data; boundary=" + separator);
  Json::Value json;
  json["name"] = filename;
  json["kind"] = "FILE";
  json["parents"].append(directory.id());
  std::string json_data = Json::FastWriter().write(json);
  json_data.pop_back();
  input_stream
      << "--" << separator << "\r\n"
      << "Content-Disposition: form-data; name=\"metadata\"\r\n\r\n"
      << json_data << "\r\n"
      << "--" << separator << "\r\n"
      << "Content-Disposition: form-data; name=\"content\"; filename=\""
      << filename << "\"\r\n"
      << "Content-Type: application/octet-stream\r\n\r\n"
      << stream.rdbuf() << "\r\n"
      << "\r\n"
      << "--" << separator << "--\r\n";
  return request;
}

HttpRequest::Pointer AmazonDrive::downloadFileRequest(const IItem& item,
                                                      std::ostream&) const {
  return make_unique<HttpRequest>(
      content_url() + "/nodes/" + item.id() + "/content",
      HttpRequest::Type::GET);
}

HttpRequest::Pointer AmazonDrive::deleteItemRequest(const IItem& item,
                                                    std::ostream&) const {
  return make_unique<HttpRequest>(metadata_url() + "/trash/" + item.id(),
                                  HttpRequest::Type::PUT);
}

HttpRequest::Pointer AmazonDrive::createDirectoryRequest(
    const IItem& parent, const std::string& name, std::ostream& input) const {
  auto request = make_unique<HttpRequest>(metadata_url() + "/nodes",
                                          HttpRequest::Type::POST);
  request->setHeaderParameter("Content-Type", "application/json");
  Json::Value json;
  json["name"] = name;
  json["kind"] = "FOLDER";
  json["parents"].append(parent.id());
  input << json;
  return request;
}

IItem::Pointer AmazonDrive::getItemDataResponse(std::istream& response) const {
  Json::Value json;
  response >> json;
  return toItem(json);
}

std::vector<IItem::Pointer> AmazonDrive::listDirectoryResponse(
    std::istream& stream, std::string& next_page_token) const {
  Json::Value response;
  stream >> response;

  std::vector<IItem::Pointer> result;
  for (const Json::Value& v : response["data"]) result.push_back(toItem(v));
  if (response.isMember("nextToken"))
    next_page_token = response["nextToken"].asString();

  return result;
}

IItem::FileType AmazonDrive::type(const Json::Value& v) const {
  if (v["kind"].asString() == "FOLDER") return IItem::FileType::Directory;
  if (v["contentProperties"].isMember("image"))
    return IItem::FileType::Image;
  else if (v["contentProperties"].isMember("video"))
    return IItem::FileType::Video;
  else
    return IItem::FileType::Unknown;
}

IItem::Pointer AmazonDrive::toItem(const Json::Value& v) const {
  std::string name = v["isRoot"].asBool() ? "root" : v["name"].asString();
  auto item = make_unique<Item>(name, v["id"].asString(), type(v));
  item->set_url(v["tempLink"].asString());
  if (item->type() == IItem::FileType::Image)
    item->set_thumbnail_url(item->url() + "?viewBox=" +
                            std::to_string(THUMBNAIL_SIZE));
  for (const Json::Value& asset : v["assets"])
    if (type(asset) == IItem::FileType::Image)
      item->set_thumbnail_url(asset["tempLink"].asString() + "?viewBox=" +
                              std::to_string(THUMBNAIL_SIZE));
  std::vector<std::string> parents;
  for (const Json::Value& p : v["parents"]) parents.push_back(p.asString());
  item->set_parents(parents);
  return item;
}

std::string AmazonDrive::metadata_url() const {
  std::lock_guard<std::mutex> lock(auth_mutex());
  return metadata_url_;
}

std::string AmazonDrive::content_url() const {
  std::lock_guard<std::mutex> lock(auth_mutex());
  return content_url_;
}

bool AmazonDrive::reauthorize(int code) const {
  return HttpRequest::isClientError(code) || HttpRequest::isCurlError(code);
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

}  // namespace cloudstorage
