/*****************************************************************************
 * Dropbox.cpp : implementation of Dropbox
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

#include "Dropbox.h"

#include <json/json.h>
#include <algorithm>
#include <sstream>

#include "Utility/Item.h"
#include "Utility/Utility.h"

#include "Request/Request.h"

const std::string DROPBOXAPI_ENDPOINT = "https://api.dropboxapi.com";
const int CHUNK_SIZE = 60 * 1024 * 1024;

namespace cloudstorage {

namespace {
void upload(Request<EitherError<IItem>>::Pointer r,
            const std::string& session_id, const std::string& path, int sent,
            IUploadFileCallback* callback) {
  int size = callback->size();
  auto length = std::make_shared<int>(0);
  r->send(
      [=](util::Output stream) {
        std::vector<char> buffer(CHUNK_SIZE);
        if (sent < size) *length = callback->putData(buffer.data(), CHUNK_SIZE);
        std::string upload_url =
            "https://content.dropboxapi.com/2/files/upload_session";
        Json::Value json;
        if (sent == 0)
          upload_url += "/start";
        else if (sent + *length >= size) {
          json["commit"]["path"] = path;
          json["commit"]["mode"] = "overwrite";
          upload_url += "/finish";
        } else
          upload_url += "/append_v2";
        auto request = r->provider()->http()->create(upload_url, "POST");
        if (sent != 0) {
          json["cursor"]["session_id"] = session_id;
          json["cursor"]["offset"] = sent;
        }
        request->setHeaderParameter("Content-Type", "application/octet-stream");
        request->setHeaderParameter("Dropbox-API-Arg",
                                    util::json::to_string(json));
        stream->write(buffer.data(), *length);
        return request;
      },
      [=](EitherError<Response> e) {
        if (e.left()) return r->done(e.left());
        try {
          auto json = util::json::from_stream(e.right()->output());
          if (sent < size)
            upload(
                r,
                session_id.empty() ? json["session_id"].asString() : session_id,
                path, sent + *length, callback);
          else
            r->done(Dropbox::toItem(json));
        } catch (const Json::Exception&) {
          r->done(Error{IHttpRequest::Failure, e.right()->output().str()});
        }
      },
      [] { return std::make_shared<std::stringstream>(); },
      std::make_shared<std::stringstream>(), nullptr,
      [=](uint64_t, uint64_t now) { callback->progress(size, sent + now); },
      true);
}
}  // namespace

Dropbox::Dropbox() : CloudProvider(util::make_unique<Auth>()) {}

std::string Dropbox::name() const { return "dropbox"; }

std::string Dropbox::endpoint() const { return DROPBOXAPI_ENDPOINT; }

IItem::Pointer Dropbox::rootDirectory() const {
  return util::make_unique<Item>("/", "", IItem::UnknownSize,
                                 IItem::UnknownTimeStamp,
                                 IItem::FileType::Directory);
}

bool Dropbox::reauthorize(int code,
                          const IHttpRequest::HeaderParameters&) const {
  return code == IHttpRequest::Bad || code == IHttpRequest::Unauthorized;
}

ICloudProvider::UploadFileRequest::Pointer Dropbox::uploadFileAsync(
    IItem::Pointer parent, const std::string& filename,
    IUploadFileCallback::Pointer cb) {
  auto callback = cb.get();
  return std::make_shared<Request<EitherError<IItem>>>(
             shared_from_this(), [=](EitherError<IItem> e) { cb->done(e); },
             [=](Request<EitherError<IItem>>::Pointer r) {
               upload(r, "", parent->id() + "/" + filename, 0, callback);
             })
      ->run();
}

ICloudProvider::GeneralDataRequest::Pointer Dropbox::getGeneralDataAsync(
    GeneralDataCallback callback) {
  auto resolver = [=](Request<EitherError<GeneralData>>::Pointer r) {
    r->request(
        [=](util::Output) {
          auto r = http()->create(endpoint() + "/2/users/get_current_account",
                                  "POST");
          r->setHeaderParameter("Content-Type", "");
          return r;
        },
        [=](EitherError<Response> e) {
          if (e.left()) return r->done(e.left());
          r->request(
              [=](util::Output) {
                auto r = http()->create(endpoint() + "/2/users/get_space_usage",
                                        "POST");
                r->setHeaderParameter("Content-Type", "");
                return r;
              },
              [=](EitherError<Response> d) {
                if (d.left()) return r->done(d.left());
                try {
                  auto json1 = util::json::from_stream(e.right()->output());
                  auto json2 = util::json::from_stream(d.right()->output());
                  GeneralData result;
                  result.username_ = json1["email"].asString();
                  result.space_total_ =
                      json2["allocation"]["allocated"].asUInt64();
                  result.space_used_ = json2["used"].asUInt64();
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

IHttpRequest::Pointer Dropbox::getItemUrlRequest(const IItem& item,
                                                 std::ostream& input) const {
  auto request =
      http()->create(endpoint() + "/2/files/get_temporary_link", "POST");
  request->setHeaderParameter("Content-Type", "application/json");
  Json::Value parameter;
  parameter["path"] = item.id();
  input << util::json::to_string(parameter);
  return request;
}

std::string Dropbox::getItemUrlResponse(const IItem&,
                                        const IHttpRequest::HeaderParameters&,
                                        std::istream& output) const {
  return util::json::from_stream(output)["link"].asString();
}

IHttpRequest::Pointer Dropbox::getItemDataRequest(const std::string& id,
                                                  std::ostream& input) const {
  auto request = http()->create(endpoint() + "/2/files/get_metadata", "POST");
  request->setHeaderParameter("Content-Type", "application/json");
  Json::Value parameter;
  parameter["path"] = id;
  input << util::json::to_string(parameter);
  return request;
}

IItem::Pointer Dropbox::getItemDataResponse(std::istream& stream) const {
  return toItem(util::json::from_stream(stream));
}

IHttpRequest::Pointer Dropbox::listDirectoryRequest(
    const IItem& item, const std::string& page_token,
    std::ostream& input_stream) const {
  if (!page_token.empty()) {
    auto request =
        http()->create(endpoint() + "/2/files/list_folder/continue", "POST");
    Json::Value input;
    input["cursor"] = page_token;
    input_stream << input;
    return request;
  }
  auto request = http()->create(endpoint() + "/2/files/list_folder", "POST");
  request->setHeaderParameter("Content-Type", "application/json");

  Json::Value parameter;
  parameter["path"] = item.id();
  input_stream << util::json::to_string(parameter);
  return request;
}

void Dropbox::authorizeRequest(IHttpRequest& r) const {
  r.setHeaderParameter("Authorization", "Bearer " + token());
}

IHttpRequest::Pointer Dropbox::downloadFileRequest(const IItem& item,
                                                   std::ostream&) const {
  auto request =
      http()->create("https://content.dropboxapi.com/2/files/download", "POST");
  request->setHeaderParameter("Content-Type", "");
  Json::Value parameter;
  parameter["path"] = item.id();
  request->setHeaderParameter("Dropbox-API-arg",
                              util::json::to_string(parameter));
  return request;
}

IHttpRequest::Pointer Dropbox::getThumbnailRequest(const IItem& item,
                                                   std::ostream&) const {
  const std::vector<std::string> supported_extensions = {
      "jpg", "jpeg", "png", "tiff", "tif", "gif", "bmp"};
  if (std::find(supported_extensions.begin(), supported_extensions.end(),
                item.extension()) == supported_extensions.end())
    return nullptr;
  auto request = http()->create(
      "https://content.dropboxapi.com/2/files/get_thumbnail", "POST");
  request->setHeaderParameter("Content-Type", "");

  Json::Value parameter;
  parameter["path"] = item.id();
  request->setHeaderParameter("Dropbox-API-arg",
                              util::json::to_string(parameter));
  return request;
}

IHttpRequest::Pointer Dropbox::deleteItemRequest(
    const IItem& item, std::ostream& input_stream) const {
  auto request = http()->create(endpoint() + "/2/files/delete", "POST");
  request->setHeaderParameter("Content-Type", "application/json");
  Json::Value parameter;
  parameter["path"] = item.id();
  input_stream << parameter;
  return request;
}

IHttpRequest::Pointer Dropbox::createDirectoryRequest(
    const IItem& item, const std::string& name, std::ostream& input) const {
  auto request =
      http()->create(endpoint() + "/2/files/create_folder_v2", "POST");
  request->setHeaderParameter("Content-Type", "application/json");
  Json::Value parameter;
  parameter["path"] = item.id() + "/" + name;
  input << parameter;
  return request;
}

IHttpRequest::Pointer Dropbox::moveItemRequest(const IItem& source,
                                               const IItem& destination,
                                               std::ostream& stream) const {
  auto request = http()->create(endpoint() + "/2/files/move_v2", "POST");
  request->setHeaderParameter("Content-Type", "application/json");
  Json::Value json;
  json["from_path"] = source.id();
  json["to_path"] = destination.id() + "/" + source.filename();
  stream << json;
  return request;
}

IHttpRequest::Pointer Dropbox::renameItemRequest(const IItem& item,
                                                 const std::string& name,
                                                 std::ostream& stream) const {
  auto request = http()->create(endpoint() + "/2/files/move_v2", "POST");
  request->setHeaderParameter("Content-Type", "application/json");
  Json::Value json;
  json["from_path"] = item.id();
  json["to_path"] = getPath(item.id()) + "/" + name;
  stream << json;
  return request;
}

IItem::List Dropbox::listDirectoryResponse(const IItem&, std::istream& stream,
                                           std::string& next_page_token) const {
  auto response = util::json::from_stream(stream);
  IItem::List result;
  for (const Json::Value& v : response["entries"]) result.push_back(toItem(v));
  if (response["has_more"].asBool()) {
    next_page_token = response["cursor"].asString();
  }
  return result;
}

IItem::Pointer Dropbox::createDirectoryResponse(const IItem&,
                                                const std::string&,
                                                std::istream& response) const {
  auto item = toItem(util::json::from_stream(response)["metadata"]);
  static_cast<Item*>(item.get())->set_type(IItem::FileType::Directory);
  return item;
}

IItem::Pointer Dropbox::renameItemResponse(const IItem& i, const std::string&,
                                           std::istream& response) const {
  auto item = toItem(util::json::from_stream(response)["metadata"]);
  static_cast<Item*>(item.get())->set_type(i.type());
  return item;
}

IItem::Pointer Dropbox::moveItemResponse(const IItem& source, const IItem&,
                                         std::istream& response) const {
  auto item = toItem(util::json::from_stream(response)["metadata"]);
  static_cast<Item*>(item.get())->set_type(source.type());
  return item;
}

IItem::Pointer Dropbox::toItem(const Json::Value& v) {
  IItem::FileType type = IItem::FileType::Unknown;
  if (v[".tag"].asString() == "folder") type = IItem::FileType::Directory;
  return util::make_unique<Item>(
      v["name"].asString(), v["path_display"].asString(),
      v.isMember("size") ? v["size"].asUInt64() : IItem::UnknownSize,
      util::parse_time(v["client_modified"].asString()), type);
}

void Dropbox::Auth::initialize(IHttp* http, IHttpServerFactory* factory) {
  cloudstorage::Auth::initialize(http, factory);
  if (client_id().empty()) {
    set_client_id("ktryxp68ae5cicj");
    set_client_secret("6evu94gcxnmyr59");
  }
}

std::string Dropbox::Auth::authorizeLibraryUrl() const {
  std::string url = "https://www.dropbox.com/oauth2/authorize?";
  url += "response_type=code&";
  url += "client_id=" + client_id() + "&";
  url += "redirect_uri=" + redirect_uri() + "&";
  url += "state=" + state();
  return url;
}

IAuth::Token::Pointer Dropbox::Auth::fromTokenString(
    const std::string& str) const {
  Token::Pointer token = util::make_unique<Token>();
  token->token_ = str;
  token->refresh_token_ = str;
  token->expires_in_ = -1;
  return token;
}

IHttpRequest::Pointer Dropbox::Auth::exchangeAuthorizationCodeRequest(
    std::ostream&) const {
  IHttpRequest::Pointer request =
      http()->create(DROPBOXAPI_ENDPOINT + "/oauth2/token", "POST");
  request->setParameter("grant_type", "authorization_code");
  request->setParameter("client_id", client_id());
  request->setParameter("client_secret", client_secret());
  request->setParameter("redirect_uri", redirect_uri());
  request->setParameter("code", authorization_code());
  return request;
}

IHttpRequest::Pointer Dropbox::Auth::refreshTokenRequest(std::ostream&) const {
  return nullptr;
}

IAuth::Token::Pointer Dropbox::Auth::exchangeAuthorizationCodeResponse(
    std::istream& stream) const {
  Token::Pointer token = util::make_unique<Token>();
  token->token_ = util::json::from_stream(stream)["access_token"].asString();
  token->refresh_token_ = token->token_;
  token->expires_in_ = -1;
  return token;
}

IAuth::Token::Pointer Dropbox::Auth::refreshTokenResponse(std::istream&) const {
  return nullptr;
}

}  // namespace cloudstorage
