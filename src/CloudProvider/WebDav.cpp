/*****************************************************************************
 * WebDav.cpp : WebDav implementation
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

#include "WebDav.h"

#include "Request/AuthorizeRequest.h"
#include "Utility/Item.h"

#include <json/json.h>
#include <cstring>
#include <iomanip>
#include <iostream>

namespace cloudstorage {

namespace {

IItem::TimeStamp parse_time(const std::string& str) {
  std::stringstream stream(str);
  std::tm time;
  stream >> std::get_time(&time, "%a, %d %b %Y %T GMT");
  if (!stream.fail()) {
    return std::chrono::system_clock::time_point(
        std::chrono::seconds(util::timegm(time)));
  } else {
    return IItem::UnknownTimeStamp;
  }
}

bool ends_with(const char* str, const char* pattern) {
  auto l1 = strlen(str);
  auto l2 = strlen(pattern);
  if (l1 < l2) return false;
  for (auto i = 0u; i < l2; i++)
    if (str[i + l1 - l2] != pattern[i]) return false;
  return true;
}

const tinyxml2::XMLElement* find(const tinyxml2::XMLElement* parent,
                                 const char* name, bool except = true) {
  for (auto element = parent->FirstChildElement(); element;
       element = element->NextSiblingElement()) {
    if (element->Name() && ends_with(element->Name(), name)) return element;
  }
  if (except) throw std::logic_error(util::Error::INVALID_XML);
  return nullptr;
}

}  // namespace

WebDav::WebDav() : CloudProvider(util::make_unique<Auth>()) {}

IItem::Pointer WebDav::rootDirectory() const {
  return util::make_unique<Item>("root", "/", IItem::UnknownSize,
                                 IItem::UnknownTimeStamp,
                                 IItem::FileType::Directory);
}

void WebDav::initialize(InitData&& data) {
  if (data.token_.empty())
    data.token_ = credentialsToString(Json::Value(Json::objectValue));
  unpackCredentials(data.token_);
  CloudProvider::initialize(std::move(data));
}

std::string WebDav::name() const { return "webdav"; }

std::string WebDav::endpoint() const {
  auto lock = auth_lock();
  return webdav_url_;
}

std::string WebDav::token() const {
  auto lock = auth_lock();
  Json::Value json;
  json["username"] = user_;
  json["password"] = password_;
  json["webdav_url"] = webdav_url_;
  return credentialsToString(json);
}

AuthorizeRequest::Pointer WebDav::authorizeAsync() {
  return std::make_shared<SimpleAuthorization>(shared_from_this());
}

IHttpRequest::Pointer WebDav::createDirectoryRequest(const IItem& parent,
                                                     const std::string& name,
                                                     std::ostream&) const {
  return http()->create(
      endpoint() + parent.id() + util::Url::escape(name) + "/", "MKCOL");
}

IItem::Pointer WebDav::createDirectoryResponse(const IItem& parent,
                                               const std::string& name,
                                               std::istream&) const {
  return util::make_unique<Item>(name, parent.id() + name + "/", 0,
                                 IItem::UnknownTimeStamp,
                                 IItem::FileType::Directory);
}

IHttpRequest::Pointer WebDav::getItemDataRequest(const std::string& id,
                                                 std::ostream&) const {
  auto request = http()->create(endpoint() + id, "PROPFIND");
  request->setHeaderParameter("Depth", "0");
  return request;
}

IHttpRequest::Pointer WebDav::listDirectoryRequest(const IItem& item,
                                                   const std::string&,
                                                   std::ostream&) const {
  auto request = http()->create(endpoint() + item.id(), "PROPFIND");
  request->setHeaderParameter("Depth", "1");
  return request;
}

IHttpRequest::Pointer WebDav::uploadFileRequest(const IItem& directory,
                                                const std::string& filename,
                                                std::ostream&,
                                                std::ostream&) const {
  return http()->create(
      endpoint() + directory.id() + util::Url::escape(filename), "PUT");
}

IHttpRequest::Pointer WebDav::downloadFileRequest(const IItem& item,
                                                  std::ostream&) const {
  return http()->create(static_cast<const Item&>(item).url(), "GET");
}

IHttpRequest::Pointer WebDav::deleteItemRequest(const IItem& item,
                                                std::ostream&) const {
  auto request = http()->create(endpoint() + item.id(), "DELETE");
  return request;
}

IHttpRequest::Pointer WebDav::moveItemRequest(const IItem& source,
                                              const IItem& destination,
                                              std::ostream&) const {
  auto request = http()->create(endpoint() + source.id(), "MOVE");
  request->setHeaderParameter(
      "Destination",
      util::Url(endpoint()).path() + destination.id() + source.filename());
  return request;
}

IHttpRequest::Pointer WebDav::renameItemRequest(const IItem& item,
                                                const std::string& name,
                                                std::ostream&) const {
  auto request = http()->create(endpoint() + item.id(), "MOVE");
  request->setHeaderParameter(
      "Destination",
      util::Url(endpoint()).path() + getPath(item.id()) + "/" + name);
  return request;
}

IHttpRequest::Pointer WebDav::getGeneralDataRequest(
    std::ostream& stream) const {
  auto request = http()->create(endpoint() + "/", "PROPFIND");
  request->setHeaderParameter("Depth", "0");
  request->setHeaderParameter("Content-Type", "text/xml");
  stream << "<D:propfind xmlns:D=\"DAV:\">"
            "  <D:prop>"
            "    <D:quota-available-bytes/>"
            "    <D:quota-used-bytes/>"
            "  </D:prop>"
            "</D:propfind>";
  return request;
}

IItem::Pointer WebDav::uploadFileResponse(const IItem& item,
                                          const std::string& filename,
                                          uint64_t size, std::istream&) const {
  return util::make_unique<Item>(filename, item.id() + filename, size,
                                 std::chrono::system_clock::now(),
                                 IItem::FileType::Unknown);
}

GeneralData WebDav::getGeneralDataResponse(std::istream& stream) const {
  std::stringstream sstream;
  sstream << stream.rdbuf();
  tinyxml2::XMLDocument document;
  if (document.Parse(sstream.str().c_str()) != tinyxml2::XML_SUCCESS)
    throw std::logic_error(util::Error::FAILED_TO_PARSE_XML);
  auto response = find(document.RootElement(), "response");
  auto propstat = find(response, "propstat");
  auto prop = find(propstat, "prop");
  auto quota_used = find(prop, "quota-used-bytes");
  auto quota_available = find(prop, "quota-available-bytes");
  GeneralData data;
  data.space_used_ =
      quota_used->GetText() ? std::stoull(quota_used->GetText()) : 0;
  data.space_total_ =
      quota_available->GetText() ? std::stoull(quota_available->GetText()) : 0;
  auto lock = auth_lock();
  auto url = util::Url(webdav_url_);
  data.username_ = url.protocol() + "://" + user_ + "@" + url.host() +
                   (url.path().empty() ? "" : "/" + url.path()) +
                   (url.query().empty() ? "" : "?" + url.query());
  return data;
}

IItem::Pointer WebDav::getItemDataResponse(std::istream& stream) const {
  std::stringstream sstream;
  sstream << stream.rdbuf();
  tinyxml2::XMLDocument document;
  if (document.Parse(sstream.str().c_str()) != tinyxml2::XML_SUCCESS)
    throw std::logic_error(util::Error::FAILED_TO_PARSE_XML);
  return toItem(document.RootElement()->FirstChildElement());
}

IItem::Pointer WebDav::renameItemResponse(const IItem& item,
                                          const std::string& name,
                                          std::istream&) const {
  auto i = util::make_unique<Item>(name, getPath(item.id()) + "/" + name,
                                   item.size(), item.timestamp(), item.type());
  auto lock = auth_lock();
  auto url = util::Url(webdav_url_);
  i->set_url(url.protocol() + "://" + user_ + ":" + password_ + "@" +
             url.host() + url.path() + i->id() + url.query());
  return std::move(i);
}

IItem::Pointer WebDav::moveItemResponse(const IItem& source, const IItem& dest,
                                        std::istream&) const {
  auto i =
      util::make_unique<Item>(source.filename(), dest.id() + source.filename(),
                              source.size(), source.timestamp(), source.type());
  auto lock = auth_lock();
  auto url = util::Url(webdav_url_);
  i->set_url(url.protocol() + "://" + user_ + ":" + password_ + "@" +
             url.host() + url.path() + i->id() + url.query());
  return std::move(i);
}

IItem::List WebDav::listDirectoryResponse(const IItem&, std::istream& stream,
                                          std::string&) const {
  std::stringstream sstream;
  sstream << stream.rdbuf();
  tinyxml2::XMLDocument document;
  if (document.Parse(sstream.str().c_str()) != tinyxml2::XML_SUCCESS)
    throw std::logic_error(util::Error::FAILED_TO_PARSE_XML);
  if (document.RootElement()->FirstChild() == nullptr) return {};

  IItem::List result;
  for (auto child = document.RootElement()->FirstChild()->NextSiblingElement();
       child; child = child->NextSiblingElement()) {
    result.push_back(toItem(child));
  }
  return result;
}

IItem::Pointer WebDav::toItem(const tinyxml2::XMLElement* node) const {
  if (!node) throw std::logic_error(util::Error::INVALID_XML);
  auto element = find(node, "href");
  auto propstat = find(node, "propstat");
  auto prop = find(propstat, "prop");
  auto size = IItem::UnknownSize;
  auto timestamp = IItem::UnknownTimeStamp;
  if (auto size_element = find(prop, "getcontentlength", false))
    if (auto text = size_element->GetText()) size = std::stoull(text);
  if (auto timestamp_element = find(prop, "getlastmodified"))
    if (auto text = timestamp_element->GetText()) timestamp = parse_time(text);
  auto lock = auth_lock();
  auto url = util::Url(webdav_url_);
  std::string id = element->GetText();
  id = id.substr(url.path().length());
  IItem::FileType type = IItem::FileType::Unknown;
  if (id.back() == '/') type = IItem::FileType::Directory;
  std::string filename = id;
  if (filename.back() == '/') filename.pop_back();
  filename = filename.substr(filename.find_last_of('/') + 1);
  auto item = util::make_unique<Item>(util::Url::unescape(filename), id, size,
                                      timestamp, type);
  item->set_url(url.protocol() + "://" + user_ + ":" + password_ + "@" +
                url.host() + url.path() + id + url.query());
  return std::move(item);
}

bool WebDav::reauthorize(int code,
                         const IHttpRequest::HeaderParameters& h) const {
  return CloudProvider::reauthorize(code, h) || endpoint().empty();
}

void WebDav::authorizeRequest(IHttpRequest& r) const {
  auto lock = auth_lock();
  r.setHeaderParameter("Authorization",
                       "Basic " + util::to_base64(user_ + ":" + password_));
}

bool WebDav::unpackCredentials(const std::string& code) {
  auto lock = auth_lock();
  try {
    auto json = credentialsFromString(code);
    user_ = json["username"].asString();
    password_ = json["password"].asString();
    webdav_url_ = json["webdav_url"].asString();
    return true;
  } catch (const Json::Exception&) {
    return false;
  }
}

std::string WebDav::Auth::authorizeLibraryUrl() const {
  return redirect_uri() + "/login?state=" + state();
}

IHttpRequest::Pointer WebDav::Auth::exchangeAuthorizationCodeRequest(
    std::ostream&) const {
  return nullptr;
}

IHttpRequest::Pointer WebDav::Auth::refreshTokenRequest(std::ostream&) const {
  return nullptr;
}

IAuth::Token::Pointer WebDav::Auth::exchangeAuthorizationCodeResponse(
    std::istream&) const {
  return nullptr;
}

IAuth::Token::Pointer WebDav::Auth::refreshTokenResponse(std::istream&) const {
  return nullptr;
}

}  // namespace cloudstorage
