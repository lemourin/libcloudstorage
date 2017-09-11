/*****************************************************************************
 * PCloud.cpp
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
#include "PCloud.h"

#include "Request/DownloadFileRequest.h"
#include "Utility/Item.h"
#include "Utility/Utility.h"

const std::string THUMBNAIL_SIZE = "64x64";

namespace cloudstorage {

PCloud::PCloud() : CloudProvider(util::make_unique<Auth>()) {}

IItem::Pointer PCloud::rootDirectory() const {
  return util::make_unique<Item>("/", "0", IItem::UnknownSize,
                                 IItem::UnknownTimeStamp,
                                 IItem::FileType::Directory);
}

std::string PCloud::name() const { return "pcloud"; }

std::string PCloud::endpoint() const { return "https://api.pcloud.com"; }

bool PCloud::isSuccess(int code,
                       const IHttpRequest::HeaderParameters& h) const {
  return IHttpRequest::isSuccess(code) && h.find("x-error") == h.end();
}

bool PCloud::reauthorize(int, const IHttpRequest::HeaderParameters& h) const {
  auto it = h.find("x-error");
  return (it != h.end() && (it->second == "1000" || it->second == "2000" ||
                            it->second == "2094"));
}

IHttpRequest::Pointer PCloud::getItemUrlRequest(const IItem& item,
                                                std::ostream&) const {
  auto r = http()->create(endpoint() + "/getfilelink");
  r->setParameter("fileid", item.id());
  return r;
}

std::string PCloud::getItemUrlResponse(const IItem&,
                                       std::istream& response) const {
  Json::Value json;
  response >> json;
  return "https://" + json["hosts"][0].asString() + json["path"].asString();
}

IHttpRequest::Pointer PCloud::getItemDataRequest(const std::string& id,
                                                 std::ostream&) const {
  auto r = http()->create(endpoint() + "/checksumfile");
  r->setParameter("fileid", id);
  r->setParameter("timeformat", "timestamp");
  return r;
}

IItem::Pointer PCloud::getItemDataResponse(std::istream& response) const {
  Json::Value json;
  response >> json;
  return toItem(json["metadata"]);
}

IItem::Pointer PCloud::uploadFileResponse(const IItem&, const std::string&,
                                          uint64_t,
                                          std::istream& response) const {
  Json::Value json;
  response >> json;
  return toItem(json["metadata"][0]);
}

IHttpRequest::Pointer PCloud::listDirectoryRequest(const IItem& item,
                                                   const std::string&,
                                                   std::ostream&) const {
  auto req = http()->create(endpoint() + "/listfolder");
  req->setParameter("folderid", item.id());
  req->setParameter("timeformat", "timestamp");
  return req;
}

ICloudProvider::DownloadFileRequest::Pointer PCloud::downloadFileAsync(
    IItem::Pointer i, IDownloadFileCallback::Pointer cb, Range range) {
  return std::make_shared<DownloadFileFromUrlRequest>(shared_from_this(), i, cb,
                                                      range)
      ->run();
}

IHttpRequest::Pointer PCloud::uploadFileRequest(const IItem& directory,
                                                const std::string& filename,
                                                std::ostream&,
                                                std::ostream&) const {
  auto req = http()->create(endpoint() + "/uploadfile", "POST");
  req->setHeaderParameter("Content-Type", "application/octet-stream");
  req->setParameter("folderid", directory.id());
  req->setParameter("filename", util::Url::escape(filename));
  req->setParameter("timeformat", "timestamp");
  return req;
}

IHttpRequest::Pointer PCloud::deleteItemRequest(const IItem& item,
                                                std::ostream&) const {
  if (item.type() == IItem::FileType::Directory) {
    auto request = http()->create(endpoint() + "/deletefolderrecursive");
    request->setParameter("folderid", item.id());
    return request;
  } else {
    auto request = http()->create(endpoint() + "/deletefile");
    request->setParameter("fileid", item.id());
    return request;
  }
}

IHttpRequest::Pointer PCloud::createDirectoryRequest(const IItem& item,
                                                     const std::string& name,
                                                     std::ostream&) const {
  auto request = http()->create(endpoint() + "/createfolder");
  request->setParameter("folderid", item.id());
  request->setParameter("name", util::Url::escape(name));
  request->setParameter("timeformat", "timestamp");
  return request;
}

IHttpRequest::Pointer PCloud::moveItemRequest(const IItem& source,
                                              const IItem& destination,
                                              std::ostream&) const {
  if (source.type() == IItem::FileType::Directory) {
    auto request = http()->create(endpoint() + "/renamefolder");
    request->setParameter("folderid", source.id());
    request->setParameter("tofolderid", destination.id());
    request->setParameter("timeformat", "timestamp");
    return request;
  } else {
    auto request = http()->create(endpoint() + "/renamefile");
    request->setParameter("fileid", source.id());
    request->setParameter("tofolderid", destination.id());
    request->setParameter("timeformat", "timestamp");
    return request;
  }
}

IHttpRequest::Pointer PCloud::renameItemRequest(const IItem& item,
                                                const std::string& name,
                                                std::ostream&) const {
  if (item.type() == IItem::FileType::Directory) {
    auto request = http()->create(endpoint() + "/renamefolder");
    request->setParameter("folderid", item.id());
    request->setParameter("toname", util::Url::escape(name));
    request->setParameter("timeformat", "timestamp");
    return request;
  } else {
    auto request = http()->create(endpoint() + "/renamefile");
    request->setParameter("fileid", item.id());
    request->setParameter("toname", util::Url::escape(name));
    request->setParameter("timeformat", "timestamp");
    return request;
  }
}

std::vector<IItem::Pointer> PCloud::listDirectoryResponse(
    const IItem&, std::istream& response, std::string&) const {
  Json::Value json;
  response >> json;
  std::vector<IItem::Pointer> result;
  for (auto&& v : json["metadata"]["contents"]) result.push_back(toItem(v));
  return result;
}

IItem::Pointer PCloud::toItem(const Json::Value& v) const {
  auto item = util::make_unique<Item>(
      v["name"].asString(),
      v["isfolder"].asBool() ? v["folderid"].asString()
                             : v["fileid"].asString(),
      v.isMember("size") ? v["size"].asInt64() : IItem::UnknownSize,
      v.isMember("modified")
          ? std::chrono::system_clock::time_point(
                std::chrono::seconds(v["modified"].asInt64()))
          : IItem::UnknownTimeStamp,
      v["isfolder"].asBool() ? IItem::FileType::Directory
                             : IItem::FileType::Unknown);
  if (v["thumb"].asBool())
    item->set_thumbnail_url(endpoint() + "/getthumb?fileid=" + item->id() +
                            "&size=" + THUMBNAIL_SIZE);
  return std::move(item);
}

PCloud::Auth::Auth() {
  set_client_id("EDEqpUpRnFF");
  set_client_secret("PQLFPbyObs5IzeUdXF7VcfkuAPs7");
}

std::string PCloud::Auth::authorizeLibraryUrl() const {
  return "https://my.pcloud.com/oauth2/authorize?client_id=" + client_id() +
         "&response_type=code"
         "&redirect_uri=" +
         util::Url::escape(redirect_uri()) + "&state=" + state();
}

IHttpRequest::Pointer PCloud::Auth::exchangeAuthorizationCodeRequest(
    std::ostream&) const {
  return http()->create(
      "https://api.pcloud.com/oauth2_token?client_id=" + client_id() +
      "&client_secret=" + client_secret() + "&code=" + authorization_code());
}

IHttpRequest::Pointer PCloud::Auth::refreshTokenRequest(std::ostream&) const {
  return nullptr;
}

IAuth::Token::Pointer PCloud::Auth::exchangeAuthorizationCodeResponse(
    std::istream& stream) const {
  Json::Value json;
  stream >> json;
  if (!json.isMember("access_token")) throw std::logic_error("no access token");
  return util::make_unique<Token>(Token{json["access_token"].asString(),
                                        json["access_token"].asString(), -1});
}

IAuth::Token::Pointer PCloud::Auth::refreshTokenResponse(std::istream&) const {
  return nullptr;
}

}  // namespace cloudstorage