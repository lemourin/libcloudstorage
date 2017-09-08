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

YandexDisk::YandexDisk() : CloudProvider(util::make_unique<Auth>()) {}

std::string YandexDisk::name() const { return "yandex"; }

std::string YandexDisk::endpoint() const {
  return "https://cloud-api.yandex.net";
}

IItem::Pointer YandexDisk::rootDirectory() const {
  return util::make_unique<Item>("disk", "disk:/", IItem::UnknownSize,
                                 IItem::UnknownTimeStamp,
                                 IItem::FileType::Directory);
}

ICloudProvider::GetItemDataRequest::Pointer YandexDisk::getItemDataAsync(
    const std::string& id, GetItemDataCallback callback) {
  auto r = std::make_shared<Request<EitherError<IItem>>>(shared_from_this());
  r->set(
      [=](Request<EitherError<IItem>>::Pointer r) {
        auto output = std::make_shared<std::stringstream>();
        r->sendRequest(
            [=](util::Output) {
              auto request =
                  http()->create(endpoint() + "/v1/disk/resources", "GET");
              request->setParameter("path", id);
              return request;
            },
            [=](EitherError<util::Output> e) {
              if (e.left()) return r->done(e.left());
              try {
                Json::Value json;
                *output >> json;
                auto item = toItem(json);
                if (item->type() != IItem::FileType::Directory) {
                  r->sendRequest(
                      [=](util::Output) {
                        auto request = http()->create(
                            endpoint() + "/v1/disk/resources/download", "GET");
                        request->setParameter("path", id);
                        return request;
                      },
                      [=](EitherError<util::Output> e) {
                        try {
                          if (!e.left()) {
                            Json::Value json;
                            *output >> json;
                            static_cast<Item*>(item.get())
                                ->set_url(json["href"].asString());
                          }
                          r->done(item);
                        } catch (std::exception) {
                          r->done(Error{IHttpRequest::Failure, output->str()});
                        }
                      },
                      output);
                } else
                  r->done(item);
              } catch (std::exception) {
                r->done(Error{IHttpRequest::Failure, output->str()});
              }
            },
            output);
      },
      callback);
  return r->run();
}

ICloudProvider::DownloadFileRequest::Pointer YandexDisk::downloadFileAsync(
    IItem::Pointer item, IDownloadFileCallback::Pointer cb, Range range) {
  auto r = std::make_shared<Request<EitherError<void>>>(shared_from_this());
  auto callback = cb.get();
  r->set(
      [=](Request<EitherError<void>>::Pointer r) {
        auto output = std::make_shared<std::stringstream>();
        r->sendRequest(
            [=](util::Output) {
              auto request = http()->create(
                  endpoint() + "/v1/disk/resources/download", "GET");
              request->setParameter("path", item->id());
              return request;
            },
            [=](EitherError<util::Output> e) {
              if (e.left()) return r->done(e.left());
              try {
                Json::Value json;
                *output >> json;
                auto wrapper = std::make_shared<DownloadStreamWrapper>(
                    std::bind(&IDownloadFileCallback::receivedData, callback,
                              _1, _2));
                auto stream = std::make_shared<std::ostream>(wrapper.get());
                std::string url = json["href"].asString();
                r->sendRequest(
                    [=](util::Output) {
                      auto r = http()->create(url, "GET");
                      if (range != FullRange)
                        r->setHeaderParameter("Range",
                                              util::range_to_string(range));
                      return r;
                    },
                    [=](EitherError<util::Output> e) {
                      (void)wrapper;
                      if (e.left())
                        r->done(e.left());
                      else
                        r->done(nullptr);
                    },
                    stream, std::bind(&IDownloadFileCallback::progress,
                                      callback, _1, _2));
              } catch (std::exception) {
                r->done(Error{IHttpRequest::Failure, output->str()});
              }
            },
            output);
      },
      [=](EitherError<void> e) { cb->done(e); });
  return r->run();
}

ICloudProvider::UploadFileRequest::Pointer YandexDisk::uploadFileAsync(
    IItem::Pointer directory, const std::string& filename,
    IUploadFileCallback::Pointer callback) {
  auto r = std::make_shared<Request<EitherError<void>>>(shared_from_this());
  r->set(
      [=](Request<EitherError<void>>::Pointer r) {
        auto output = std::make_shared<std::stringstream>();
        r->sendRequest(
            [=](util::Output) {
              auto request = http()->create(
                  endpoint() + "/v1/disk/resources/upload", "GET");
              std::string path = directory->id();
              if (path.back() != '/') path += "/";
              path += filename;
              request->setParameter("path", path);
              return request;
            },
            [=](EitherError<util::Output> e) {
              if (e.left()) return r->done(e.left());
              try {
                Json::Value response;
                *output >> response;
                std::string url = response["href"].asString();
                auto wrapper = std::make_shared<UploadStreamWrapper>(
                    std::bind(&IUploadFileCallback::putData, callback.get(), _1,
                              _2),
                    callback->size());
                auto output = std::make_shared<std::stringstream>();
                r->sendRequest(
                    [=](util::Output input) {
                      auto request = http()->create(url, "PUT");
                      callback->reset();
                      wrapper->reset();
                      input->rdbuf(wrapper.get());
                      return request;
                    },
                    [=](EitherError<util::Output> e) {
                      (void)wrapper;
                      if (e.left())
                        r->done(e.left());
                      else
                        r->done(nullptr);
                    },
                    output, nullptr, std::bind(&IUploadFileCallback::progress,
                                               callback.get(), _1, _2));
              } catch (std::exception) {
                r->done(Error{IHttpRequest::Failure, output->str()});
              }
            },
            output);
      },
      [=](EitherError<void> e) { callback->done(e); });
  return r->run();
}

ICloudProvider::CreateDirectoryRequest::Pointer
YandexDisk::createDirectoryAsync(IItem::Pointer parent, const std::string& name,
                                 CreateDirectoryCallback callback) {
  auto r = std::make_shared<Request<EitherError<IItem>>>(shared_from_this());
  r->set(
      [=](Request<EitherError<IItem>>::Pointer r) {
        auto output = std::make_shared<std::stringstream>();
        r->sendRequest(
            [=](util::Output) {
              auto request =
                  http()->create(endpoint() + "/v1/disk/resources/", "PUT");
              request->setParameter(
                  "path",
                  parent->id() + (parent->id().back() == '/' ? "" : "/") +
                      name);
              return request;
            },
            [=](EitherError<util::Output> e) {
              if (e.left()) return r->done(e.left());
              try {
                Json::Value json;
                *output >> json;
                r->sendRequest(
                    [=](util::Output) {
                      auto request =
                          http()->create(json["href"].asString(), "GET");
                      return request;
                    },
                    [=](EitherError<util::Output> e) {
                      if (e.left()) return r->done(e.left());
                      try {
                        Json::Value json;
                        *output >> json;
                        r->done(toItem(json));
                      } catch (std::exception) {
                        r->done(Error{IHttpRequest::Failure, output->str()});
                      }
                    },
                    output);
              } catch (std::exception) {
                r->done(Error{IHttpRequest::Failure, output->str()});
              }
            },
            output);
      },
      callback);
  return r->run();
}

IHttpRequest::Pointer YandexDisk::listDirectoryRequest(
    const IItem& item, const std::string& page_token, std::ostream&) const {
  auto request = http()->create(endpoint() + "/v1/disk/resources", "GET");
  request->setParameter("path", item.id());
  if (!page_token.empty()) request->setParameter("offset", page_token);
  return request;
}

IHttpRequest::Pointer YandexDisk::deleteItemRequest(const IItem& item,
                                                    std::ostream&) const {
  auto request = http()->create(endpoint() + "/v1/disk/resources", "DELETE");
  request->setParameter("path", item.id());
  request->setParameter("permamently", "true");
  return request;
}

IHttpRequest::Pointer YandexDisk::moveItemRequest(const IItem& source,
                                                  const IItem& destination,
                                                  std::ostream&) const {
  auto request = http()->create(endpoint() + "/v1/disk/resources/move", "POST");
  request->setParameter("from", source.id());
  request->setParameter("path",
                        destination.id() +
                            (destination.id().back() == '/' ? "" : "/") +
                            source.filename());
  return request;
}

IHttpRequest::Pointer YandexDisk::renameItemRequest(const IItem& item,
                                                    const std::string& name,
                                                    std::ostream&) const {
  auto request = http()->create(endpoint() + "/v1/disk/resources/move", "POST");
  request->setParameter("from", item.id());
  request->setParameter("path", getPath(item.id()) + "/" + name);
  return request;
}

std::vector<IItem::Pointer> YandexDisk::listDirectoryResponse(
    const IItem&, std::istream& stream, std::string& next_page_token) const {
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
  auto item = util::make_unique<Item>(
      v["name"].asString(), v["path"].asString(),
      v.isMember("size") ? v["size"].asUInt64() : IItem::UnknownSize,
      util::parse_time(v["modified"].asString()), type);
  item->set_thumbnail_url(v["preview"].asString());
  return std::move(item);
}

void YandexDisk::authorizeRequest(IHttpRequest& request) const {
  request.setHeaderParameter("Authorization", "OAuth " + token());
}

YandexDisk::Auth::Auth() {
  set_client_id("04d700d432884c4381c07e760213ed8a");
  set_client_secret("197f9693caa64f0ebb51d201110074f9");
}

std::string YandexDisk::Auth::authorizeLibraryUrl() const {
  return "https://oauth.yandex.com/authorize?response_type=code&client_id=" +
         client_id() + "&state=" + state();
}

IHttpRequest::Pointer YandexDisk::Auth::exchangeAuthorizationCodeRequest(
    std::ostream& input_data) const {
  auto request = http()->create("https://oauth.yandex.com/token", "POST");
  input_data << "grant_type=authorization_code&"
             << "client_id=" << client_id() << "&"
             << "client_secret=" << client_secret() << "&"
             << "code=" << authorization_code();
  return request;
}

IHttpRequest::Pointer YandexDisk::Auth::refreshTokenRequest(
    std::ostream&) const {
  return nullptr;
}

IAuth::Token::Pointer YandexDisk::Auth::exchangeAuthorizationCodeResponse(
    std::istream& stream) const {
  Json::Value response;
  stream >> response;
  auto token = util::make_unique<Token>();
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
