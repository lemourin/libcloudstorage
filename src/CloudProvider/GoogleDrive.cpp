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

#include <jsoncpp/json/json.h>
#include <algorithm>
#include <sstream>

#include "Utility/HttpRequest.h"
#include "Utility/Item.h"
#include "Utility/Utility.h"

namespace cloudstorage {

GoogleDrive::GoogleDrive() : CloudProvider(make_unique<Auth>()) {}

std::string GoogleDrive::name() const { return "google"; }

HttpRequest::Pointer GoogleDrive::listDirectoryRequest(const IItem& f,
                                                       std::ostream&) const {
  const Item& item = static_cast<const Item&>(f);
  HttpRequest::Pointer request = make_unique<HttpRequest>(
      "https://www.googleapis.com/drive/v3/files", HttpRequest::Type::GET);
  request->setParameter("q", std::string("'") + item.id() + "'+in+parents");
  request->setParameter("fields",
                        "files(id,name,thumbnailLink,trashed,webContentLink,"
                        "mimeType,iconLink),kind,"
                        "nextPageToken");
  return request;
}

HttpRequest::Pointer GoogleDrive::uploadFileRequest(
    const IItem& f, const std::string& filename, std::istream& stream,
    std::ostream& input_stream) const {
  const std::string separator = "fWoDm9QNn3v3Bq3bScUX";
  const Item& item = static_cast<const Item&>(f);
  HttpRequest::Pointer request = make_unique<HttpRequest>(
      "https://www.googleapis.com/upload/drive/v3/files?uploadType=multipart",
      HttpRequest::Type::POST);
  request->setHeaderParameter("Content-Type",
                              "multipart/related; boundary=" + separator);
  Json::Value request_data;
  request_data["name"] = filename;
  request_data["parents"].append(item.id());
  std::string json_data = Json::FastWriter().write(request_data);
  json_data.pop_back();
  input_stream << "--" << separator << "\r\n"
               << "Content-Type: application/json; charset=UTF-8\r\n\r\n"
               << json_data << "\r\n"
               << "--" << separator << "\r\n"
               << "Content-Type: \r\n\r\n"
               << stream.rdbuf() << "\r\n"
               << "--" << separator << "--";
  return request;
}

HttpRequest::Pointer GoogleDrive::downloadFileRequest(const IItem& f,
                                                      std::ostream&) const {
  const Item& item = static_cast<const Item&>(f);
  HttpRequest::Pointer request = make_unique<HttpRequest>(
      "https://www.googleapis.com/drive/v3/files/" + item.id(),
      HttpRequest::Type::GET);
  request->setParameter("alt", "media");
  return request;
}

HttpRequest::Pointer GoogleDrive::getThumbnailRequest(const IItem& f,
                                                      std::ostream&) const {
  const Item& item = static_cast<const Item&>(f);
  HttpRequest::Pointer request =
      make_unique<HttpRequest>(item.thumbnail_url(), HttpRequest::Type::GET);
  return request;
}

std::vector<IItem::Pointer> GoogleDrive::listDirectoryResponse(
    std::istream& stream, HttpRequest::Pointer& next_page_request,
    std::ostream&) const {
  Json::Value response;
  stream >> response;
  std::vector<IItem::Pointer> result;
  for (Json::Value v : response["files"]) {
    auto item = make_unique<Item>(v["name"].asString(), v["id"].asString(),
                                  toFileType(v["mimeType"].asString()));
    item->set_hidden(v["trashed"].asBool());
    std::string thumnail_url = v["thumbnailLink"].asString();
    if (!thumnail_url.empty() && isGoogleMimeType(v["mimeType"].asString()))
      thumnail_url += "&access_token=" + access_token();
    else if (thumnail_url.empty())
      thumnail_url = v["iconLink"].asString();
    item->set_thumbnail_url(thumnail_url);
    item->set_url("https://www.googleapis.com/drive/v3/files/" + item->id() +
                  "?alt=media&access_token=" + access_token());
    result.push_back(std::move(item));
  }

  if (response.isMember("nextPageToken"))
    next_page_request->setParameter("pageToken",
                                    response["nextPageToken"].asString());
  else
    next_page_request = nullptr;
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
  else if (mime_type == "application/vnd.google-apps.audio")
    return IItem::FileType::Audio;
  else if (mime_type == "application/vnd.google-apps.video")
    return IItem::FileType::Video;
  else if (mime_type == "application/vnd.google-apps.photo")
    return IItem::FileType::Image;
  else
    return IItem::FileType::Unknown;
}

GoogleDrive::Auth::Auth() {
  set_client_id(
      "646432077068-hmvk44qgo6d0a64a5h9ieue34p3j2dcv.apps.googleusercontent."
      "com");
  set_client_secret("1f0FG5ch-kKOanTAv1Bqdp9U");
}

std::string GoogleDrive::Auth::authorizeLibraryUrl() const {
  std::string response_type = "code";
  std::string scope = "https://www.googleapis.com/auth/drive";
  std::string url = "https://accounts.google.com/o/oauth2/auth?";
  url += "response_type=" + response_type + "&";
  url += "client_id=" + client_id() + "&";
  url += "redirect_uri=" + redirect_uri() + "&";
  url += "scope=" + scope;
  return url;
}

HttpRequest::Pointer GoogleDrive::Auth::exchangeAuthorizationCodeRequest(
    std::ostream& arguments) const {
  HttpRequest::Pointer request = make_unique<HttpRequest>(
      "https://accounts.google.com/o/oauth2/token", HttpRequest::Type::POST);
  arguments << "grant_type=authorization_code&"
            << "client_secret=" << client_secret() << "&"
            << "code=" << authorization_code() << "&"
            << "client_id=" << client_id() << "&"
            << "redirect_uri=" << redirect_uri();
  return request;
}

HttpRequest::Pointer GoogleDrive::Auth::refreshTokenRequest(
    std::ostream& arguments) const {
  HttpRequest::Pointer request = make_unique<HttpRequest>(
      "https://accounts.google.com/o/oauth2/token", HttpRequest::Type::POST);
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

  Token::Pointer token = make_unique<Token>();
  token->token_ = response["access_token"].asString();
  token->refresh_token_ = response["refresh_token"].asString();
  token->expires_in_ = response["expires_in"].asInt();
  return token;
}

IAuth::Token::Pointer GoogleDrive::Auth::refreshTokenResponse(
    std::istream& data) const {
  Json::Value response;
  data >> response;
  Token::Pointer token = make_unique<Token>();
  token->token_ = response["access_token"].asString();
  token->refresh_token_ = access_token()->refresh_token_;
  token->expires_in_ = response["expires_in"].asInt();
  return token;
}

}  // namespace cloudstorage
