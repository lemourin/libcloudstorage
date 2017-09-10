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

#include "Utility/Item.h"

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

bool PCloud::reauthorize(int, const IHttpRequest::HeaderParameters& h) const {
  auto it = h.find("x-error");
  return (it != h.end() && (it->second == "1000" || it->second == "2000"));
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
  return r;
}

IItem::Pointer PCloud::getItemDataResponse(std::istream& response) const {
  Json::Value json;
  response >> json;
  return toItem(json["metadata"]);
}

IHttpRequest::Pointer PCloud::listDirectoryRequest(const IItem& item,
                                                   const std::string&,
                                                   std::ostream&) const {
  auto req = http()->create(endpoint() + "/listfolder");
  req->setParameter("folderid", item.id());
  return req;
}

IHttpRequest::Pointer PCloud::uploadFileRequest(const IItem&,
                                                const std::string&,
                                                std::ostream&,
                                                std::ostream&) const {
  return nullptr;
}

IHttpRequest::Pointer PCloud::downloadFileRequest(const IItem&,
                                                  std::ostream&) const {
  return nullptr;
}

IHttpRequest::Pointer PCloud::deleteItemRequest(const IItem&,
                                                std::ostream&) const {
  return nullptr;
}

IHttpRequest::Pointer PCloud::createDirectoryRequest(const IItem&,
                                                     const std::string&,
                                                     std::ostream&) const {
  return nullptr;
}

IHttpRequest::Pointer PCloud::moveItemRequest(const IItem&, const IItem&,
                                              std::ostream&) const {
  return nullptr;
}

IHttpRequest::Pointer PCloud::renameItemRequest(const IItem&,
                                                const std::string&,
                                                std::ostream&) const {
  return nullptr;
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
      IItem::UnknownSize, IItem::UnknownTimeStamp,
      v["isfolder"].asBool() ? IItem::FileType::Directory
                             : IItem::FileType::Unknown);
  if (v["thumb"].asBool())
    item->set_thumbnail_url(endpoint() + "/getthumb?fileid=" + item->id() +
                            "&size=" + THUMBNAIL_SIZE);
  return item;
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
