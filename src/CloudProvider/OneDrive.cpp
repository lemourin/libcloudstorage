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

#include <iostream>
#include "Request/Request.h"
#include "Utility/Item.h"
#include "Utility/Utility.h"

const uint32_t CHUNK_SIZE = 60 * 1024 * 1024;
using namespace std::placeholders;

namespace cloudstorage {

namespace {
void upload(Request<EitherError<IItem>>::Pointer r,
            const std::string& upload_url, int sent,
            IUploadFileCallback* callback, Json::Value response) {
  auto size = callback->size();
  auto length = std::make_shared<int>(0);
  if (sent >= size)
    return r->done(
        static_cast<OneDrive*>(r->provider().get())->toItem(response));
  r->send(
      [=](util::Output stream) {
        std::vector<char> buffer(CHUNK_SIZE);
        *length = callback->putData(buffer.data(), CHUNK_SIZE, sent);
        auto request = r->provider()->http()->create(upload_url, "PUT");
        std::stringstream content_range;
        content_range << "bytes " << sent << "-" << sent + *length - 1 << "/"
                      << size;
        request->setHeaderParameter("Content-Range", content_range.str());
        stream->write(buffer.data(), *length);
        return request;
      },
      [=](EitherError<Response> e) {
        if (e.left()) return r->done(e.left());
        try {
          auto json = util::json::from_stream(e.right()->output());
          upload(r, upload_url, sent + *length, callback, json);
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

OneDrive::OneDrive() : CloudProvider(util::make_unique<Auth>()) {}

std::string OneDrive::name() const { return "onedrive"; }

std::string OneDrive::endpoint() const {
  auto lock = auth_lock();
  return endpoint_;
}

void OneDrive::initialize(ICloudProvider::InitData&& d) {
  setWithHint(d.hints_, "endpoint", [=](std::string v) {
    auto lock = auth_lock();
    endpoint_ = v;
  });
  CloudProvider::initialize(std::move(d));
}

ICloudProvider::Hints OneDrive::hints() const {
  auto hints = CloudProvider::hints();
  hints.insert({{"endpoint", endpoint()}});
  return hints;
}

bool OneDrive::reauthorize(int code,
                           const IHttpRequest::HeaderParameters& h) const {
  auto lock = auth_lock();
  return CloudProvider::reauthorize(code, h) || endpoint_.empty();
}

AuthorizeRequest::Pointer OneDrive::authorizeAsync() {
  return util::make_unique<AuthorizeRequest>(
      shared_from_this(), [=](AuthorizeRequest::Pointer r,
                              AuthorizeRequest::AuthorizeCompleted complete) {
        r->oauth2Authorization([=](EitherError<void> e) {
          if (e.left()) return complete(e.left());
          r->query(
              [=](util::Output) {
                auto request =
                    http()->create("https://graph.microsoft.com/v1.0/me");
                authorizeRequest(*request);
                return request;
              },
              [=](Response e) {
                if (!IHttpRequest::isSuccess(e.http_code())) {
                  return complete(Error{e.http_code(), e.error_output().str()});
                }
                try {
                  auto json = util::json::from_stream(e.output());
                  auto lock = auth_lock();
                  if (json.isMember("mySite")) {
                    endpoint_ = json["mySite"].asString() + "_api/v2.0";
                  } else {
                    endpoint_ = "https://graph.microsoft.com/v1.0";
                  }
                  lock.unlock();
                  complete(nullptr);
                } catch (const Json::Exception& e) {
                  complete(Error{IHttpRequest::Failure, e.what()});
                }
              });
        });
      });
}

ICloudProvider::UploadFileRequest::Pointer OneDrive::uploadFileAsync(
    IItem::Pointer parent, const std::string& filename,
    IUploadFileCallback::Pointer cb) {
  auto callback = cb.get();
  return std::make_shared<Request<EitherError<IItem>>>(
             shared_from_this(), [=](EitherError<IItem> e) { cb->done(e); },
             [=](Request<EitherError<IItem>>::Pointer r) {
               r->request(
                   [=](util::Output) {
                     return http()->create(endpoint() + "/me/drive/items/" +
                                               parent->id() + ":/" +
                                               util::Url::escape(filename) +
                                               ":/createUploadSession",
                                           "POST");
                   },
                   [=](EitherError<Response> e) {
                     if (e.left()) return r->done(e.left());
                     try {
                       auto response =
                           util::json::from_stream(e.right()->output());
                       upload(r, response["uploadUrl"].asString(), 0, callback,
                              response);
                     } catch (const Json::Exception& e) {
                       r->done(Error{IHttpRequest::Failure, e.what()});
                     }
                   });
             })
      ->run();
}

ICloudProvider::GeneralDataRequest::Pointer OneDrive::getGeneralDataAsync(
    GeneralDataCallback callback) {
  auto resolver = [=](Request<EitherError<GeneralData>>::Pointer r) {
    r->request(
        [=](util::Output) { return http()->create(endpoint() + "/me/drive"); },
        [=](EitherError<Response> e) {
          if (e.left()) return r->done(e.left());
          r->request(
              [=](util::Output) { return http()->create(endpoint() + "/me"); },
              [=](EitherError<Response> d) {
                if (d.left()) return r->done(d.left());
                try {
                  auto json1 = util::json::from_stream(e.right()->output());
                  auto json2 = util::json::from_stream(d.right()->output());
                  GeneralData result;
                  result.space_total_ = json1["quota"]["total"].asUInt64();
                  result.space_used_ = json1["quota"]["used"].asUInt64();
                  result.username_ = json2["userPrincipalName"].asString();
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

IHttpRequest::Pointer OneDrive::getItemDataRequest(const std::string& id,
                                                   std::ostream&) const {
  IHttpRequest::Pointer request =
      http()->create(endpoint() + "/drive/items/" + id, "GET");
  request->setParameter("select",
                        "name,folder,audio,image,photo,video,id,size,"
                        "lastModifiedDateTime,thumbnails,@content.downloadUrl");
  request->setParameter("expand", "thumbnails");
  return request;
}

IHttpRequest::Pointer OneDrive::listDirectoryRequest(
    const IItem& item, const std::string& page_token, std::ostream&) const {
  if (!page_token.empty()) return http()->create(page_token, "GET");
  auto request = http()->create(
      endpoint() + "/drive/items/" + item.id() + "/children", "GET");
  request->setParameter("select",
                        "name,folder,audio,image,photo,video,id,size,"
                        "lastModifiedDateTime,thumbnails,@content.downloadUrl");
  request->setParameter("expand", "thumbnails");
  return request;
}

IHttpRequest::Pointer OneDrive::downloadFileRequest(const IItem& f,
                                                    std::ostream&) const {
  const Item& item = static_cast<const Item&>(f);
  auto request = http()->create(
      endpoint() + "/drive/items/" + item.id() + "/content", "GET");
  return request;
}

IHttpRequest::Pointer OneDrive::deleteItemRequest(const IItem& item,
                                                  std::ostream&) const {
  return http()->create(endpoint() + "/drive/items/" + item.id(), "DELETE");
}

IHttpRequest::Pointer OneDrive::createDirectoryRequest(
    const IItem& parent, const std::string& name, std::ostream& input) const {
  auto request = http()->create(
      endpoint() + "/drive/items/" + parent.id() + "/children", "POST");
  request->setHeaderParameter("Content-Type", "application/json");
  Json::Value json;
  json["name"] = name;
  json["folder"] = Json::Value(Json::objectValue);
  input << json;
  return request;
}

IHttpRequest::Pointer OneDrive::moveItemRequest(const IItem& source,
                                                const IItem& destination,
                                                std::ostream& stream) const {
  auto request =
      http()->create(endpoint() + "/drive/items/" + source.id(), "PATCH");
  request->setHeaderParameter("Content-Type", "application/json");
  Json::Value json;
  if (destination.id() == rootDirectory()->id())
    json["parentReference"]["path"] = "/drive/root";
  else
    json["parentReference"]["id"] = destination.id();
  stream << json;
  return request;
}

IHttpRequest::Pointer OneDrive::renameItemRequest(const IItem& item,
                                                  const std::string& name,
                                                  std::ostream& stream) const {
  auto request =
      http()->create(endpoint() + "/drive/items/" + item.id(), "PATCH");
  request->setHeaderParameter("Content-Type", "application/json");
  Json::Value json;
  json["name"] = name;
  stream << json;
  return request;
}

IItem::Pointer OneDrive::getItemDataResponse(std::istream& response) const {
  return toItem(util::json::from_stream(response));
}

IItem::Pointer OneDrive::toItem(const Json::Value& v) const {
  IItem::FileType type = IItem::FileType::Unknown;
  if (v.isMember("folder"))
    type = IItem::FileType::Directory;
  else if (v.isMember("video"))
    type = IItem::FileType::Video;
  else if (v.isMember("image") || v.isMember("photo"))
    type = IItem::FileType::Image;
  else if (v.isMember("audio"))
    type = IItem::FileType::Audio;
  auto item = util::make_unique<Item>(
      v["name"].asString(), v["id"].asString(), v["size"].asUInt64(),
      util::parse_time(v["lastModifiedDateTime"].asString()), type);
  item->set_url(v["@microsoft.graph.downloadUrl"].asString());
  item->set_thumbnail_url(v["thumbnails"][0]["small"]["url"].asString());
  return std::move(item);
}

IItem::List OneDrive::listDirectoryResponse(
    const IItem&, std::istream& stream, std::string& next_page_token) const {
  IItem::List result;
  auto response = util::json::from_stream(stream);
  for (Json::Value v : response["value"]) result.push_back(toItem(v));
  if (response.isMember("@odata.nextLink"))
    next_page_token = response["@odata.nextLink"].asString();
  return result;
}

void OneDrive::Auth::initialize(IHttp* http, IHttpServerFactory* factory) {
  cloudstorage::Auth::initialize(http, factory);
  if (client_id().empty()) {
    set_client_id("56a1d60f-ea71-40e9-a489-b87fba12a23e");
    set_client_secret("zJRAsd0o4E9c33q4OLc7OhY");
  }
}

std::string OneDrive::Auth::authorizeLibraryUrl() const {
  std::string result = std::string(
      "https://login.microsoftonline.com/common/oauth2/v2.0/authorize?");
  std::string scope = "offline_access%20user.read%20";
  if (permission() == ICloudProvider::Permission::ReadWrite)
    scope += "files.readwrite";
  else
    scope += "files.read";
  std::string response_type = "code";
  result += "client_id=" + util::Url::escape(client_id()) + "&";
  result += "scope=" + scope + "&";
  result += "response_type=" + response_type + "&";
  result += "redirect_uri=" + util::Url::escape(redirect_uri()) + "&";
  result += "state=" + state();
  return result;
}

IHttpRequest::Pointer OneDrive::Auth::exchangeAuthorizationCodeRequest(
    std::ostream& data) const {
  auto request = http()->create(
      "https://login.microsoftonline.com/common/oauth2/v2.0/token", "POST");
  data << "client_id=" << util::Url::escape(client_id()) << "&"
       << "client_secret=" << util::Url::escape(client_secret()) << "&"
       << "redirect_uri=" << util::Url::escape(redirect_uri()) << "&"
       << "code=" << util::Url::escape(authorization_code()) << "&"
       << "grant_type=authorization_code";
  return request;
}

IHttpRequest::Pointer OneDrive::Auth::refreshTokenRequest(
    std::ostream& data) const {
  IHttpRequest::Pointer request = http()->create(
      "https://login.microsoftonline.com/common/oauth2/v2.0/token", "POST");
  data << "client_id=" << util::Url::escape(client_id()) << "&"
       << "client_secret=" << util::Url::escape(client_secret()) << "&"
       << "refresh_token=" << util::Url::escape(access_token()->refresh_token_)
       << "&grant_type=refresh_token";
  return request;
}

IAuth::Token::Pointer OneDrive::Auth::exchangeAuthorizationCodeResponse(
    std::istream& stream) const {
  auto response = util::json::from_stream(stream);
  auto token = util::make_unique<Token>();
  token->token_ = response["access_token"].asString();
  token->refresh_token_ = response["refresh_token"].asString();
  token->expires_in_ = response["expires_in"].asInt();
  return token;
}

IAuth::Token::Pointer OneDrive::Auth::refreshTokenResponse(
    std::istream& stream) const {
  auto response = util::json::from_stream(stream);
  auto token = util::make_unique<Token>();
  token->token_ = response["access_token"].asString();
  token->refresh_token_ = response["refresh_token"].asString();
  token->expires_in_ = response["expires_in"].asInt();
  return token;
}

}  // namespace cloudstorage
