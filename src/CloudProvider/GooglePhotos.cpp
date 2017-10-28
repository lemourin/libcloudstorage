/*****************************************************************************
 * GooglePhotos.cpp
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
#include "GooglePhotos.h"

#include <json/json.h>
#include "Request/DownloadFileRequest.h"
#include "Utility/Item.h"
#include "Utility/Utility.h"

namespace cloudstorage {

namespace {

struct ItemId {
  std::string album_id_;
  std::string id_;
};

std::string to_string(const ItemId &id) {
  Json::Value json;
  json["a"] = id.album_id_;
  if (!id.id_.empty()) json["i"] = id.id_;
  return util::to_base64(util::json::to_string(json));
}

ItemId from_string(const std::string &str) {
  std::stringstream stream(util::from_base64(str));
  auto json = util::json::from_stream(stream);
  return {json["a"].asString(), json["i"].asString()};
}

}  // namespace

GooglePhotos::GooglePhotos() : CloudProvider(util::make_unique<Auth>()) {}

std::string GooglePhotos::name() const { return "gphotos"; }

std::string GooglePhotos::endpoint() const {
  return "https://picasaweb.google.com/data";
}

IItem::Pointer GooglePhotos::rootDirectory() const {
  return util::make_unique<Item>("/", "e30K", IItem::UnknownSize,
                                 IItem::UnknownTimeStamp,
                                 IItem::FileType::Directory);
}

bool GooglePhotos::reauthorize(int code,
                               const IHttpRequest::HeaderParameters &h) const {
  return code == IHttpRequest::Forbidden || CloudProvider::reauthorize(code, h);
}

IHttpRequest::Pointer GooglePhotos::getItemDataRequest(
    const std::string &id_str, std::ostream &) const {
  if (id_str == rootDirectory()->id()) return nullptr;
  IHttpRequest::Pointer request;
  auto id = from_string(id_str);
  if (id.id_.empty()) {
    request = http()->create(endpoint() + "/feed/api/user/default/albumid/" +
                             id.album_id_);
    request->setParameter(
        "fields",
        util::Url::escape("gphoto:id,title,gphoto:bytesUsed,gphoto:timestamp"));
  } else {
    request = http()->create(endpoint() + "/feed/api/user/default/albumid/" +
                             id.album_id_ + "/photoid/" + id.id_);
    request->setParameter("imgmax", "d");
    request->setParameter(
        "fields", util::Url::escape(
                      "title,gphoto:id,gphoto:timestamp,"
                      "gphoto:size,gphoto:albumid,media:group(media:content)"));
  }
  request->setParameter("GData-Version", "3");
  return request;
}

IItem::Pointer GooglePhotos::getItemDataResponse(std::istream &response) const {
  std::stringstream stream;
  stream << response.rdbuf();
  tinyxml2::XMLDocument document;
  if (document.Parse(stream.str().c_str()) != tinyxml2::XML_SUCCESS)
    throw std::logic_error("invalid xml");
  return toItem(document.RootElement());
}

IHttpRequest::Pointer GooglePhotos::listDirectoryRequest(const IItem &directory,
                                                         const std::string &,
                                                         std::ostream &) const {
  IHttpRequest::Pointer request;
  if (directory.id() == rootDirectory()->id()) {
    request = http()->create(endpoint() + "/feed/api/user/default");
    request->setParameter(
        "fields",
        util::Url::escape("entry(id,title,content,gphoto:size,gphoto:bytesUsed,"
                          "gphoto:timestamp,gphoto:id,gphoto:albumid,"
                          "media:group(media:"
                          "thumbnail),link[@rel='http://schemas.google.com/g/"
                          "2005#feed'](@href))"));
  } else {
    request = http()->create(endpoint() + "/feed/api/user/default/albumid/" +
                             from_string(directory.id()).album_id_);
    request->setParameter("kind", "photo");
    request->setParameter("imgmax", "d");
    request->setParameter(
        "fields",
        util::Url::escape(
            "entry(id,title,content,gphoto:size,gphoto:bytesUsed,"
            "gphoto:timestamp,gphoto:id,gphoto:albumid,"
            "media:group(media:"
            "thumbnail,media:content),link[@rel='http://schemas.google.com/g/"
            "2005#feed'](@href))"));
  }

  request->setHeaderParameter("GData-Version", "3");
  return request;
}

IHttpRequest::Pointer GooglePhotos::uploadFileRequest(
    const IItem &directory, const std::string &filename, std::ostream &,
    std::ostream &) const {
  auto request = http()->create(endpoint() + "/feed/api/user/default/albumid/" +
                                    from_string(directory.id()).album_id_,
                                "POST");
  request->setHeaderParameter("Slug", filename);
  request->setHeaderParameter("Content-Type", "image/png");
  request->setParameter("imgmax", "d");
  request->setHeaderParameter("GData-Version", "3");
  return request;
}

std::vector<IItem::Pointer> GooglePhotos::listDirectoryResponse(
    const IItem &, std::istream &stream, std::string &) const {
  std::stringstream sstream;
  sstream << stream.rdbuf();
  tinyxml2::XMLDocument document;
  if (document.Parse(sstream.str().c_str(), sstream.str().size()) !=
      tinyxml2::XML_SUCCESS)
    throw std::logic_error("invalid xml");
  std::vector<IItem::Pointer> result;
  for (auto child = document.RootElement()->FirstChildElement("entry"); child;
       child = child->NextSiblingElement("entry")) {
    result.push_back(toItem(child));
  }
  return result;
}

ICloudProvider::DownloadFileRequest::Pointer GooglePhotos::downloadFileAsync(
    IItem::Pointer i, IDownloadFileCallback::Pointer cb, Range range) {
  return std::make_shared<DownloadFileFromUrlRequest>(shared_from_this(), i, cb,
                                                      range)
      ->run();
}

IItem::Pointer GooglePhotos::toItem(const tinyxml2::XMLElement *child) const {
  auto title = child->FirstChildElement("title");
  auto media_group = child->FirstChildElement("media:group");
  auto content =
      media_group ? media_group->FirstChildElement("media:content") : nullptr;
  auto id = child->FirstChildElement("gphoto:id");
  auto size = child->FirstChildElement("gphoto:size");
  auto album_size = child->FirstChildElement("gphoto:bytesUsed");
  auto timestamp = child->FirstChildElement("gphoto:timestamp");
  auto album = child->FirstChildElement("gphoto:albumid");
  if (content) {
    auto item = std::make_shared<Item>(
        title->GetText(), to_string({album->GetText(), id->GetText()}),
        std::stoull(size->GetText()),
        std::chrono::system_clock::time_point(
            std::chrono::milliseconds(std::stoull(timestamp->GetText()))),
        IItem::FileType::Image);
    item->set_thumbnail_url(content->Attribute("url"));
    item->set_url(content->Attribute("url"));

    return item;
  } else {
    auto item = std::make_shared<Item>(
        title->GetText(), to_string({id->GetText(), ""}),
        album_size ? std::stoull(album_size->GetText()) : IItem::UnknownSize,
        std::chrono::system_clock::time_point(
            std::chrono::milliseconds(std::stoull(timestamp->GetText()))),
        IItem::FileType::Directory);
    return item;
  }
}

std::string GooglePhotos::Auth::authorizeLibraryUrl() const {
  return "https://accounts.google.com/o/oauth2/auth?client_id=" + client_id() +
         "&redirect_uri=" + redirect_uri() +
         "&scope=https://picasaweb.google.com/data/"
         "&response_type=code&access_type=offline&prompt=consent"
         "&state=" +
         state();
}

}  // namespace cloudstorage
