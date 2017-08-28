/*****************************************************************************
 * GoogleDrive.cpp : implementation of GoogleDrive
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

#include "GoogleDrive.h"

#include <json/json.h>
#include <algorithm>
#include <sstream>

#include "Utility/Item.h"
#include "Utility/Utility.h"

const std::string GOOGLEAPI_ENDPOINT = "https://www.googleapis.com";

namespace cloudstorage {

GoogleDrive::GoogleDrive() : CloudProvider(util::make_unique<Auth>()) {}

std::string GoogleDrive::name() const { return "google"; }

std::string GoogleDrive::endpoint() const { return GOOGLEAPI_ENDPOINT; }

IHttpRequest::Pointer GoogleDrive::getItemDataRequest(const std::string& id,
                                                      std::ostream&) const {
  auto request = http()->create(endpoint() + "/drive/v3/files/" + id, "GET");
  request->setParameter("fields",
                        "id,name,thumbnailLink,trashed,"
                        "mimeType,iconLink,parents,size,modifiedTime");
  return request;
}

IHttpRequest::Pointer GoogleDrive::listDirectoryRequest(
    const IItem& item, const std::string& page_token, std::ostream&) const {
  IHttpRequest::Pointer request =
      http()->create(endpoint() + "/drive/v3/files", "GET");
  request->setParameter("q", std::string("'") + item.id() + "'+in+parents");
  request->setParameter("fields",
                        "files(id,name,thumbnailLink,trashed,"
                        "mimeType,iconLink,parents,size,modifiedTime),kind,"
                        "nextPageToken");
  if (!page_token.empty()) request->setParameter("pageToken", page_token);
  return request;
}

IHttpRequest::Pointer GoogleDrive::uploadFileRequest(
    const IItem& f, const std::string& filename, std::ostream& prefix_stream,
    std::ostream& suffix_stream) const {
  const std::string separator = "fWoDm9QNn3v3Bq3bScUX";
  const Item& item = static_cast<const Item&>(f);
  IHttpRequest::Pointer request = http()->create(
      endpoint() + "/upload/drive/v3/files?uploadType=multipart", "POST");
  request->setHeaderParameter("Content-Type",
                              "multipart/related; boundary=" + separator);
  Json::Value request_data;
  request_data["name"] = filename;
  request_data["parents"].append(item.id());
  std::string json_data = Json::FastWriter().write(request_data);
  json_data.pop_back();
  prefix_stream << "--" << separator << "\r\n"
                << "Content-Type: application/json; charset=UTF-8\r\n\r\n"
                << json_data << "\r\n"
                << "--" << separator << "\r\n"
                << "Content-Type: \r\n\r\n";
  suffix_stream << "\r\n--" << separator << "--\r\n";
  return request;
}

IHttpRequest::Pointer GoogleDrive::downloadFileRequest(const IItem& item,
                                                       std::ostream&) const {
  IHttpRequest::Pointer request =
      http()->create(endpoint() + "/drive/v3/files/" + item.id(), "GET");
  request->setParameter("alt", "media");
  return request;
}

IHttpRequest::Pointer GoogleDrive::deleteItemRequest(const IItem& item,
                                                     std::ostream&) const {
  return http()->create(endpoint() + "/drive/v3/files/" + item.id(), "DELETE");
}

IHttpRequest::Pointer GoogleDrive::createDirectoryRequest(
    const IItem& item, const std::string& name, std::ostream& input) const {
  auto request = http()->create(endpoint() + "/drive/v3/files", "POST");
  request->setHeaderParameter("Content-Type", "application/json");
  request->setParameter("fields",
                        "id,name,thumbnailLink,trashed,"
                        "mimeType,iconLink,parents");
  Json::Value json;
  json["mimeType"] = "application/vnd.google-apps.folder";
  json["name"] = name;
  json["parents"].append(item.id());
  input << json;
  return request;
}

IHttpRequest::Pointer GoogleDrive::moveItemRequest(const IItem& s,
                                                   const IItem& destination,
                                                   std::ostream& input) const {
  const Item& source = static_cast<const Item&>(s);
  auto request =
      http()->create(endpoint() + "/drive/v3/files/" + source.id(), "PATCH");
  request->setHeaderParameter("Content-Type", "application/json");
  std::string current_parents;
  for (auto str : source.parents()) current_parents += str + ",";
  current_parents.pop_back();
  request->setParameter("removeParents", current_parents);
  request->setParameter("addParents", destination.id());
  input << Json::Value();
  return request;
}

IHttpRequest::Pointer GoogleDrive::renameItemRequest(
    const IItem& item, const std::string& name, std::ostream& input) const {
  auto request =
      http()->create(endpoint() + "/drive/v3/files/" + item.id(), "PATCH");
  request->setHeaderParameter("Content-Type", "application/json");
  Json::Value json;
  json["name"] = name;
  input << json;
  return request;
}

IItem::Pointer GoogleDrive::getItemDataResponse(std::istream& response) const {
  Json::Value json;
  response >> json;
  return toItem(json);
}

std::vector<IItem::Pointer> GoogleDrive::listDirectoryResponse(
    const IItem&, std::istream& stream, std::string& next_page_token) const {
  Json::Value response;
  stream >> response;
  std::vector<IItem::Pointer> result;
  for (Json::Value v : response["files"]) result.push_back(toItem(v));

  if (response.isMember("nextPageToken"))
    next_page_token = response["nextPageToken"].asString();
  return result;
}

bool GoogleDrive::isGoogleMimeType(const std::string& mime_type) const {
  std::vector<std::string> types = {"application/vnd.google-apps.document",
                                    "application/vnd.google-apps.drawing",
                                    "application/vnd.google-apps.form",
                                    "application/vnd.google-apps.fusiontable",
                                    "application/vnd.google-apps.map",
                                    "application/vnd.google-apps.presentation",
                                    "application/vnd.google-apps.script",
                                    "application/vnd.google-apps.sites",
                                    "application/vnd.google-apps.spreadsheet"};
  return std::find(types.begin(), types.end(), mime_type) != types.end();
}

IItem::FileType GoogleDrive::toFileType(const std::string& mime_type) const {
  if (mime_type == "application/vnd.google-apps.folder")
    return IItem::FileType::Directory;
  else
    return Item::fromMimeType(mime_type);
}

IItem::Pointer GoogleDrive::toItem(const Json::Value& v) const {
  auto item = util::make_unique<Item>(
      v["name"].asString(), v["id"].asString(),
      v.isMember("size") ? std::atoll(v["size"].asString().c_str())
                         : IItem::UnknownSize,
      util::parse_time(v["modifiedTime"].asString()),
      toFileType(v["mimeType"].asString()));
  item->set_hidden(v["trashed"].asBool());
  std::string thumnail_url = v["thumbnailLink"].asString();
  if (!thumnail_url.empty() && isGoogleMimeType(v["mimeType"].asString()))
    thumnail_url += "&access_token=" + access_token();
  else if (thumnail_url.empty())
    thumnail_url = v["iconLink"].asString();
  item->set_thumbnail_url(thumnail_url);
  item->set_url(endpoint() + "/drive/v3/files/" + item->id() +
                "?alt=media&access_token=" + access_token());
  std::vector<std::string> parents;
  for (auto id : v["parents"]) parents.push_back(id.asString());
  item->set_parents(parents);
  return std::move(item);
}

GoogleDrive::Auth::Auth() {
  set_client_id(
      "646432077068-hmvk44qgo6d0a64a5h9ieue34p3j2dcv.apps.googleusercontent."
      "com");
  set_client_secret("1f0FG5ch-kKOanTAv1Bqdp9U");
}

std::string GoogleDrive::Auth::authorizeLibraryUrl() const {
  std::string response_type = "code";
  std::string scope = GOOGLEAPI_ENDPOINT + "/auth/drive";
  std::string url = "https://accounts.google.com/o/oauth2/auth?";
  url += "response_type=" + response_type + "&";
  url += "client_id=" + client_id() + "&";
  url += "redirect_uri=" + redirect_uri() + "&";
  url += "scope=" + scope + "&";
  url += "access_type=offline&prompt=consent&";
  url += "state=" + state();
  return url;
}

IHttpRequest::Pointer GoogleDrive::Auth::exchangeAuthorizationCodeRequest(
    std::ostream& arguments) const {
  IHttpRequest::Pointer request =
      http()->create("https://accounts.google.com/o/oauth2/token", "POST");
  arguments << "grant_type=authorization_code&"
            << "client_secret=" << client_secret() << "&"
            << "code=" << authorization_code() << "&"
            << "client_id=" << client_id() << "&"
            << "redirect_uri=" << redirect_uri();
  return request;
}

IHttpRequest::Pointer GoogleDrive::Auth::refreshTokenRequest(
    std::ostream& arguments) const {
  IHttpRequest::Pointer request =
      http()->create("https://accounts.google.com/o/oauth2/token", "POST");
  arguments << "refresh_token=" << access_token()->refresh_token_ << "&"
            << "client_id=" << client_id() << "&"
            << "client_secret=" << client_secret() << "&"
            << "redirect_uri=" << redirect_uri() << "&"
            << "grant_type=refresh_token";
  return request;
}

IAuth::Token::Pointer GoogleDrive::Auth::exchangeAuthorizationCodeResponse(
    std::istream& data) const {
  Json::Value response;
  data >> response;

  Token::Pointer token = util::make_unique<Token>();
  token->token_ = response["access_token"].asString();
  token->refresh_token_ = response["refresh_token"].asString();
  token->expires_in_ = response["expires_in"].asInt();
  return token;
}

IAuth::Token::Pointer GoogleDrive::Auth::refreshTokenResponse(
    std::istream& data) const {
  Json::Value response;
  data >> response;
  Token::Pointer token = util::make_unique<Token>();
  token->token_ = response["access_token"].asString();
  token->refresh_token_ = access_token()->refresh_token_;
  token->expires_in_ = response["expires_in"].asInt();
  return token;
}

}  // namespace cloudstorage
