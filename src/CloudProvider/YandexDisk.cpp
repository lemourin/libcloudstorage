/*****************************************************************************
 * YandexDisk.cpp : YandexDisk implementation
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

#include "YandexDisk.h"

#include <json/json.h>

#include "Request/DownloadFileRequest.h"
#include "Request/Request.h"
#include "Request/UploadFileRequest.h"
#include "Utility/Item.h"
#include "Utility/Utility.h"

using namespace std::placeholders;

namespace cloudstorage {

YandexDisk::YandexDisk() : CloudProvider(make_unique<Auth>()) {}

std::string YandexDisk::name() const { return "yandex"; }

IItem::Pointer YandexDisk::rootDirectory() const {
  return make_unique<Item>("disk", "disk:/", IItem::FileType::Directory);
}

ICloudProvider::GetItemDataRequest::Pointer YandexDisk::getItemDataAsync(
    const std::string& id, GetItemDataCallback callback) {
  auto r = make_unique<Request<IItem::Pointer>>(shared_from_this());
  r->set_resolver(
      [this, id, callback](Request<IItem::Pointer>* r) -> IItem::Pointer {
        std::stringstream output;
        int code = r->sendRequest(
            [id](std::ostream&) {
              auto request = make_unique<HttpRequest>(
                  "https://cloud-api.yandex.net/v1/disk/resources",
                  HttpRequest::Type::GET);
              request->setParameter("path", id);
              return request;
            },
            output);
        if (!HttpRequest::isSuccess(code)) {
          callback(nullptr);
          return nullptr;
        }
        Json::Value json;
        output >> json;
        auto item = toItem(json);
        if (item->type() != IItem::FileType::Directory) {
          code = r->sendRequest(
              [id](std::ostream&) {
                auto request = make_unique<HttpRequest>(
                    "https://cloud-api.yandex.net/v1/disk/resources/download",
                    HttpRequest::Type::GET);
                request->setParameter("path", id);
                return request;
              },
              output);
          if (HttpRequest::isSuccess(code)) {
            output >> json;
            static_cast<Item*>(item.get())->set_url(json["href"].asString());
          }
        }
        callback(item);
        return item;
      });
  return r;
}

ICloudProvider::DownloadFileRequest::Pointer YandexDisk::downloadFileAsync(
    IItem::Pointer item, IDownloadFileCallback::Pointer callback) {
  auto r = std::make_shared<Request<void>>(shared_from_this());
  r->set_error_callback([this, callback, r](int code, const std::string& desc) {
    callback->error(r->error_string(code, desc));
  });
  r->set_resolver([this, item, callback](Request<void>* r) -> void {
    std::stringstream output;
    int code = r->sendRequest(
        [item](std::ostream&) {
          auto request = make_unique<HttpRequest>(
              "https://cloud-api.yandex.net/v1/disk/resources/download",
              HttpRequest::Type::GET);
          request->setParameter("path", item->id());
          return request;
        },
        output);
    if (!HttpRequest::isSuccess(code))
      callback->error("Couldn't get download url.");
    else {
      Json::Value json;
      output >> json;
      DownloadStreamWrapper wrapper(std::bind(
          &IDownloadFileCallback::receivedData, callback.get(), _1, _2));
      std::ostream stream(&wrapper);
      std::string url = json["href"].asString();
      code = r->sendRequest(
          [url](std::ostream&) {
            return make_unique<HttpRequest>(url, HttpRequest::Type::GET);
          },
          stream,
          std::bind(&IDownloadFileCallback::progress, callback.get(), _1, _2));
      if (HttpRequest::isSuccess(code)) callback->done();
    }
  });
  return r;
}

ICloudProvider::UploadFileRequest::Pointer YandexDisk::uploadFileAsync(
    IItem::Pointer directory, const std::string& filename,
    IUploadFileCallback::Pointer callback) {
  auto r = std::make_shared<Request<void>>(shared_from_this());
  r->set_error_callback([this, callback, r](int code, const std::string& desc) {
    callback->error(r->error_string(code, desc));
  });
  r->set_resolver([this, directory, filename, callback](Request<void>* r) {
    std::stringstream output;
    int code = r->sendRequest(
        [directory, filename](std::ostream&) {
          auto request = make_unique<HttpRequest>(
              "https://cloud-api.yandex.net/v1/disk/resources/upload",
              HttpRequest::Type::GET);
          std::string path = directory->id();
          if (path.back() != '/') path += "/";
          path += filename;
          request->setParameter("path", path);
          return request;
        },
        output);
    if (HttpRequest::isSuccess(code)) {
      Json::Value response;
      output >> response;
      std::string url = response["href"].asString();
      UploadStreamWrapper wrapper(
          std::bind(&IUploadFileCallback::putData, callback.get(), _1, _2),
          callback->size());
      code = r->sendRequest(
          [url, callback, &wrapper](std::ostream& input) {
            auto request =
                make_unique<HttpRequest>(url, HttpRequest::Type::PUT);
            callback->reset();
            input.rdbuf(&wrapper);
            return request;
          },
          output, nullptr,
          std::bind(&IUploadFileCallback::progress, callback.get(), _1, _2));
      if (HttpRequest::isSuccess(code)) callback->done();
    } else
      callback->error("Couldn't get upload url.");
  });
  return r;
}

ICloudProvider::CreateDirectoryRequest::Pointer
YandexDisk::createDirectoryAsync(IItem::Pointer parent, const std::string& name,
                                 CreateDirectoryCallback callback) {
  auto r = make_unique<Request<IItem::Pointer>>(shared_from_this());
  r->set_resolver([=](Request<IItem::Pointer>* r) -> IItem::Pointer {
    std::stringstream output;
    int code = r->sendRequest(
        [=](std::ostream&) {
          auto request = make_unique<HttpRequest>(
              "https://cloud-api.yandex.net/v1/disk/resources/",
              HttpRequest::Type::PUT);
          request->setParameter(
              "path",
              parent->id() + (parent->id().back() == '/' ? "" : "/") + name);
          return request;
        },
        output);
    if (HttpRequest::isSuccess(code)) {
      Json::Value json;
      output >> json;
      code = r->sendRequest(
          [=](std::ostream&) {
            auto request = make_unique<HttpRequest>(json["href"].asString(),
                                                    HttpRequest::Type::GET);
            return request;
          },
          output);
      if (HttpRequest::isSuccess(code)) {
        output >> json;
        auto item = toItem(json);
        callback(item);
        return item;
      }
    }
    callback(nullptr);
    return nullptr;
  });
  return r;
}

HttpRequest::Pointer YandexDisk::listDirectoryRequest(
    const IItem& item, const std::string& page_token, std::ostream&) const {
  auto request = make_unique<HttpRequest>(
      "https://cloud-api.yandex.net/v1/disk/resources", HttpRequest::Type::GET);
  request->setParameter("path", item.id());
  if (!page_token.empty()) request->setParameter("offset", page_token);
  return request;
}

HttpRequest::Pointer YandexDisk::deleteItemRequest(const IItem& item,
                                                   std::ostream&) const {
  auto request = make_unique<HttpRequest>(
      "https://cloud-api.yandex.net/v1/disk/resources", HttpRequest::Type::DEL);
  request->setParameter("path", item.id());
  request->setParameter("permamently", "true");
  return request;
}

HttpRequest::Pointer YandexDisk::moveItemRequest(const IItem& source,
                                                 const IItem& destination,
                                                 std::ostream&) const {
  auto request = make_unique<HttpRequest>(
      "https://cloud-api.yandex.net/v1/disk/resources/move",
      HttpRequest::Type::POST);
  request->setParameter("from", source.id());
  request->setParameter(
      "path", destination.id() + (destination.id().back() == '/' ? "" : "/") +
                  source.filename());
  return request;
}

std::vector<IItem::Pointer> YandexDisk::listDirectoryResponse(
    std::istream& stream, std::string& next_page_token) const {
  Json::Value response;
  stream >> response;
  std::vector<IItem::Pointer> result;
  for (const Json::Value& v : response["_embedded"]["items"])
    result.push_back(toItem(v));
  int offset = response["_embedded"]["offset"].asInt();
  int limit = response["_embedded"]["limit"].asInt();
  int total_count = response["_embedded"]["total"].asInt();
  if (offset + limit < total_count)
    next_page_token = std::to_string(offset + limit);
  return result;
}

IItem::Pointer YandexDisk::toItem(const Json::Value& v) const {
  IItem::FileType type = v["type"].asString() == "dir"
                             ? IItem::FileType::Directory
                             : Item::fromMimeType(v["mime_type"].asString());
  auto item =
      make_unique<Item>(v["name"].asString(), v["path"].asString(), type);
  item->set_thumbnail_url(v["preview"].asString());
  return item;
}

void YandexDisk::authorizeRequest(HttpRequest& request) const {
  request.setHeaderParameter("Authorization", "OAuth " + access_token());
}

YandexDisk::Auth::Auth() {
  set_client_id("04d700d432884c4381c07e760213ed8a");
  set_client_secret("197f9693caa64f0ebb51d201110074f9");
}

std::string YandexDisk::Auth::authorizeLibraryUrl() const {
  return "https://oauth.yandex.com/authorize?response_type=code&client_id=" +
         client_id();
}

HttpRequest::Pointer YandexDisk::Auth::exchangeAuthorizationCodeRequest(
    std::ostream& input_data) const {
  auto request = make_unique<HttpRequest>("https://oauth.yandex.com/token",
                                          HttpRequest::Type::POST);
  input_data << "grant_type=authorization_code&"
             << "client_id=" << client_id() << "&"
             << "client_secret=" << client_secret() << "&"
             << "code=" << authorization_code();
  return request;
}

HttpRequest::Pointer YandexDisk::Auth::refreshTokenRequest(
    std::ostream&) const {
  return nullptr;
}

IAuth::Token::Pointer YandexDisk::Auth::exchangeAuthorizationCodeResponse(
    std::istream& stream) const {
  Json::Value response;
  stream >> response;
  auto token = make_unique<Token>();
  token->expires_in_ = -1;
  token->token_ = response["access_token"].asString();
  token->refresh_token_ = token->token_;
  return token;
}

IAuth::Token::Pointer YandexDisk::Auth::refreshTokenResponse(
    std::istream&) const {
  return nullptr;
}

}  // namespace cloudstorage
