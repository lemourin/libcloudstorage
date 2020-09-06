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

namespace {

template <class T>
void check_status(typename Request<EitherError<T>>::Pointer r,
                  const std::string& href, EitherError<T> result) {
  r->request(
      [=](util::Output) { return r->provider()->http()->create(href); },
      [=](EitherError<Response> e) {
        if (e.left()) return r->done(e.left());
        try {
          auto json = util::json::from_stream(e.right()->output());
          if (json.isMember("status")) {
            if (json["status"].asString() == "in-progress")
              check_status(r, href, result);
            else if (json["status"].asString() == "success")
              r->done(result);
            else
              r->done(Error{IHttpRequest::Failure, e.right()->output().str()});
          } else {
            r->done(result);
          }
        } catch (const Json::Exception&) {
          r->done(Error{IHttpRequest::Failure, e.right()->output().str()});
        }
      });
}

}  // namespace

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

IHttpRequest::Pointer YandexDisk::getItemUrlRequest(const IItem& item,
                                                    std::ostream&) const {
  auto request =
      http()->create(endpoint() + "/v1/disk/resources/download", "GET");
  request->setParameter("path", item.id());
  return request;
}

std::string YandexDisk::getItemUrlResponse(
    const IItem&, const IHttpRequest::HeaderParameters&,
    std::istream& response) const {
  return util::json::from_stream(response)["href"].asString();
}

IHttpRequest::Pointer YandexDisk::getItemDataRequest(const std::string& id,
                                                     std::ostream&) const {
  auto request = http()->create(endpoint() + "/v1/disk/resources", "GET");
  request->setParameter("path", id);
  return request;
}

IItem::Pointer YandexDisk::getItemDataResponse(std::istream& response) const {
  return toItem(util::json::from_stream(response));
}

ICloudProvider::DownloadFileRequest::Pointer YandexDisk::downloadFileAsync(
    IItem::Pointer i, IDownloadFileCallback::Pointer cb, Range range) {
  return std::make_shared<DownloadFileFromUrlRequest>(shared_from_this(), i, cb,
                                                      range)
      ->run();
}

ICloudProvider::RenameItemRequest::Pointer YandexDisk::renameItemAsync(
    IItem::Pointer item, const std::string& name, RenameItemCallback cb) {
  auto resolve = [=](Request<EitherError<IItem>>::Pointer r) {
    r->request(
        [=](util::Output) {
          auto request =
              http()->create(endpoint() + "/v1/disk/resources/move", "POST");
          request->setParameter("from", item->id());
          request->setParameter("path", getPath(item->id()) + "/" + name);
          return request;
        },
        [=](EitherError<Response> e) {
          if (e.left()) return r->done(e.left());
          try {
            auto json = util::json::from_stream(e.right()->output());
            if (json.isMember("href"))
              check_status<IItem>(
                  r, json["href"].asString(),
                  std::static_pointer_cast<IItem>(std::make_shared<Item>(
                      name, getPath(item->id()) + "/" + name, item->size(),
                      item->timestamp(), item->type())));
            else
              r->done(toItem(json));
          } catch (const Json::Exception& e) {
            r->done(Error{IHttpRequest::Failure, e.what()});
          }
        });
  };
  return std::make_shared<Request<EitherError<IItem>>>(shared_from_this(), cb,
                                                       resolve)
      ->run();
}

ICloudProvider::MoveItemRequest::Pointer YandexDisk::moveItemAsync(
    IItem::Pointer source, IItem::Pointer destination, MoveItemCallback cb) {
  auto resolve = [=](Request<EitherError<IItem>>::Pointer r) {
    r->request(
        [=](util::Output) {
          auto request =
              http()->create(endpoint() + "/v1/disk/resources/move", "POST");
          request->setParameter("from", source->id());
          request->setParameter(
              "path", destination->id() +
                          (destination->id().back() == '/' ? "" : "/") +
                          source->filename());
          return request;
        },
        [=](EitherError<Response> e) {
          if (e.left()) return r->done(e.left());
          try {
            auto json = util::json::from_stream(e.right()->output());
            if (json.isMember("href"))
              check_status<IItem>(
                  r, json["href"].asString(),
                  std::static_pointer_cast<IItem>(std::make_shared<Item>(
                      source->filename(),
                      destination->id() +
                          (destination->id().back() == '/' ? "" : "/") +
                          source->filename(),
                      source->size(), source->timestamp(), source->type())));
            else
              r->done(toItem(json));
          } catch (const Json::Exception& e) {
            r->done(Error{IHttpRequest::Failure, e.what()});
          }
        });
  };
  return std::make_shared<Request<EitherError<IItem>>>(shared_from_this(), cb,
                                                       resolve)
      ->run();
}

ICloudProvider::DeleteItemRequest::Pointer YandexDisk::deleteItemAsync(
    IItem::Pointer item, DeleteItemCallback cb) {
  auto resolve = [=](Request<EitherError<void>>::Pointer r) {
    r->request(
        [=](util::Output) {
          auto request =
              http()->create(endpoint() + "/v1/disk/resources", "DELETE");
          request->setParameter("path", item->id());
          request->setParameter("permamently", "true");
          return request;
        },
        [=](EitherError<Response> e) {
          if (e.left()) return r->done(e.left());
          try {
            if (e.right()->http_code() == IHttpRequest::Accepted)
              check_status<void>(
                  r,
                  util::json::from_stream(e.right()->output())["href"]
                      .asString(),
                  nullptr);
            else
              r->done(nullptr);
          } catch (const Json::Exception&) {
            r->done(Error{IHttpRequest::Failure, e.right()->output().str()});
          }
        });
  };
  return std::make_shared<Request<EitherError<void>>>(shared_from_this(), cb,
                                                      resolve)
      ->run();
}

ICloudProvider::UploadFileRequest::Pointer YandexDisk::uploadFileAsync(
    IItem::Pointer directory, const std::string& filename,
    IUploadFileCallback::Pointer callback) {
  auto path = directory->id();
  if (path.back() != '/') path += "/";
  path += filename;
  auto upload_url = [=](Request<EitherError<IItem>>::Pointer r,
                        std::function<void(EitherError<std::string>)> f) {
    r->request(
        [=](util::Output) {
          auto request =
              http()->create(endpoint() + "/v1/disk/resources/upload", "GET");
          request->setParameter("path", path);
          return request;
        },
        [=](EitherError<Response> e) {
          if (e.left()) f(e.left());
          try {
            f(util::json::from_stream(e.right()->output())["href"].asString());
          } catch (const Json::Exception& e) {
            f(Error{IHttpRequest::Failure, e.what()});
          }
        });
  };
  auto upload = [=](Request<EitherError<IItem>>::Pointer r, std::string url,
                    std::function<void(EitherError<IItem>)> f) {
    auto wrapper = std::make_shared<UploadStreamWrapper>(
        std::bind(&IUploadFileCallback::putData, callback.get(), _1, _2, _3),
        callback->size());
    r->send(
        [=](util::Output) {
          auto request = http()->create(url, "PUT");
          wrapper->reset();
          return request;
        },
        [=](EitherError<Response> e) {
          if (e.left()) return f(e.left());
          IItem::Pointer item = util::make_unique<Item>(
              filename, path, wrapper->size_, std::chrono::system_clock::now(),
              IItem::FileType::Unknown);
          f(item);
        },
        [=] { return std::make_shared<std::iostream>(wrapper.get()); },
        std::make_shared<std::stringstream>(), nullptr,
        std::bind(&IUploadFileCallback::progress, callback.get(), _1, _2),
        true);
  };
  return std::make_shared<Request<EitherError<IItem>>>(
             shared_from_this(),
             [=](EitherError<IItem> e) { callback->done(e); },
             [=](Request<EitherError<IItem>>::Pointer r) {
               upload_url(r, [=](EitherError<std::string> ret) {
                 if (ret.left()) return r->done(ret.left());
                 upload(r, *ret.right(),
                        [=](EitherError<IItem> e) { r->done(e); });
               });
             })
      ->run();
}

ICloudProvider::GeneralDataRequest::Pointer YandexDisk::getGeneralDataAsync(
    GeneralDataCallback callback) {
  auto resolver = [=](Request<EitherError<GeneralData>>::Pointer r) {
    r->request(
        [=](util::Output) {
          return http()->create("https://login.yandex.ru/info");
        },
        [=](EitherError<Response> e) {
          if (e.left()) return r->done(e.left());
          r->request(
              [=](util::Output) {
                return http()->create(endpoint() + "/v1/disk");
              },
              [=](EitherError<Response> d) {
                if (d.left()) return r->done(d.left());
                try {
                  auto json1 = util::json::from_stream(e.right()->output());
                  auto json2 = util::json::from_stream(d.right()->output());
                  GeneralData result;
                  result.username_ = json1["login"].asString();
                  result.space_total_ = json2["total_space"].asUInt64();
                  result.space_used_ = json2["used_space"].asUInt64();
                  r->done(result);
                } catch (const Json::Exception& e) {
                  r->done(Error{IHttpRequest::Failure, e.what()});
                }
              });
        });
  };
  return std::make_shared<Request<EitherError<GeneralData>>>(shared_from_this(),
                                                             callback, resolver)
      ->run();
}

IHttpRequest::Pointer YandexDisk::createDirectoryRequest(
    const IItem& parent, const std::string& name, std::ostream&) const {
  auto request = http()->create(endpoint() + "/v1/disk/resources/", "PUT");
  auto path = parent.id() + (parent.id().back() == '/' ? "" : "/") + name;
  request->setParameter("path", path);
  return request;
}

IItem::Pointer YandexDisk::createDirectoryResponse(const IItem& parent,
                                                   const std::string& name,
                                                   std::istream&) const {
  auto path = parent.id() + (parent.id().back() == '/' ? "" : "/") + name;
  return util::make_unique<Item>(name, path, 0,
                                 std::chrono::system_clock::now(),
                                 IItem::FileType::Directory);
}

IHttpRequest::Pointer YandexDisk::listDirectoryRequest(
    const IItem& item, const std::string& page_token, std::ostream&) const {
  auto request = http()->create(endpoint() + "/v1/disk/resources", "GET");
  request->setParameter("path", item.id());
  if (!page_token.empty()) request->setParameter("offset", page_token);
  return request;
}

IHttpRequest::Pointer YandexDisk::moveItemRequest(const IItem& source,
                                                  const IItem& destination,
                                                  std::ostream&) const {
  auto request = http()->create(endpoint() + "/v1/disk/resources/move", "POST");
  request->setParameter("from", source.id());
  request->setParameter(
      "path", destination.id() + (destination.id().back() == '/' ? "" : "/") +
                  source.filename());
  return request;
}

IItem::Pointer YandexDisk::moveItemResponse(const IItem& source,
                                            const IItem& dest,
                                            std::istream&) const {
  return util::make_unique<Item>(
      source.filename(),
      dest.id() + (dest.id().back() == '/' ? "" : "/") + source.filename(),
      source.size(), source.timestamp(), source.type());
}

IItem::List YandexDisk::listDirectoryResponse(
    const IItem&, std::istream& stream, std::string& next_page_token) const {
  auto response = util::json::from_stream(stream);
  IItem::List result;
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

void YandexDisk::Auth::initialize(IHttp* http, IHttpServerFactory* factory) {
  cloudstorage::Auth::initialize(http, factory);
  if (client_id().empty()) {
    if (permission() == Permission::ReadWrite) {
      set_client_id("04d700d432884c4381c07e760213ed8a");
      set_client_secret("197f9693caa64f0ebb51d201110074f9");
    } else {
      set_client_id("7000dc1c529c4791a3510231233b7865");
      set_client_secret("3879feb4f5aa4d8b971fe5c5ebfa5e25");
    }
  }
}

std::string YandexDisk::Auth::authorizeLibraryUrl() const {
  return "https://oauth.yandex.com/authorize?response_type=code&client_id=" +
         client_id() + "&state=" + state() + "&redirect_uri=" + redirect_uri();
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

IAuth::Token::Pointer YandexDisk::Auth::exchangeAuthorizationCodeResponse(
    std::istream& stream) const {
  auto response = util::json::from_stream(stream);
  auto token = util::make_unique<Token>();
  token->expires_in_ = -1;
  token->token_ = response["access_token"].asString();
  token->refresh_token_ = token->token_;
  return token;
}

}  // namespace cloudstorage
