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

#include "Request/DownloadFileRequest.h"
#include "Request/UploadFileRequest.h"
#include "Utility/Item.h"
#include "Utility/Utility.h"

const std::string GOOGLEAPI_ENDPOINT = "https://www.googleapis.com";

using namespace std::placeholders;

namespace cloudstorage {

namespace {

std::string exported_mime_type(const std::string& type) {
  if (type == "application/vnd.google-apps.document")
    return "application/"
           "vnd.openxmlformats-officedocument.wordprocessingml.document";
  else if (type == "application/vnd.google-apps.spreadsheet")
    return "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet";
  else if (type == "application/vnd.google-apps.drawing")
    return "image/png";
  else if (type == "application/vnd.google-apps.presentation")
    return "application/"
           "vnd.openxmlformats-officedocument.presentationml.presentation";
  else if (type == "application/vnd.google-apps.script")
    return "application/vnd.google-apps.script+json";
  else
    return "";
}

std::string exported_extension(const std::string& type) {
  if (type == "application/vnd.google-apps.document")
    return ".docx";
  else if (type == "application/vnd.google-apps.spreadsheet")
    return ".xlsx";
  else if (type == "application/vnd.google-apps.drawing")
    return ".png";
  else if (type == "application/vnd.google-apps.presentation")
    return ".pptx";
  else if (type == "application/vnd.google-apps.script")
    return ".json";
  else
    return "";
}

std::string google_extension_to_mime_type(const std::string& ext) {
  if (ext == ".docx")
    return "application/vnd.google-apps.document";
  else if (ext == ".xlsx")
    return "application/vnd.google-apps.spreadsheet";
  else if (ext == ".pptx")
    return "application/vnd.google-apps.presentation";
  else
    return "";
}

}  // namespace

GoogleDrive::GoogleDrive() : CloudProvider(util::make_unique<Auth>()) {}

std::string GoogleDrive::name() const { return "google"; }

std::string GoogleDrive::endpoint() const { return GOOGLEAPI_ENDPOINT; }

IHttpRequest::Pointer GoogleDrive::getItemUrlRequest(
    const IItem& item, std::ostream& stream) const {
  return getItemDataRequest(item.id(), stream);
}

std::string GoogleDrive::getItemUrlResponse(
    const IItem& item, const IHttpRequest::HeaderParameters&,
    std::istream&) const {
  const Item& i = static_cast<const Item&>(item);
  if (isGoogleMimeType(i.mime_type())) {
    return endpoint() + "/drive/v3/files/" + item.id() +
           "/export?access_token=" + access_token() +
           "&mimeType=" + exported_mime_type(i.mime_type());
  } else {
    return endpoint() + "/drive/v3/files/" + item.id() +
           "?alt=media&access_token=" + access_token();
  }
}

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
  if (item.id() == "shared")
    request->setParameter("q", "sharedWithMe");
  else
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
  return upload(f, endpoint() + "/upload/drive/v3/files", "POST", filename,
                prefix_stream, suffix_stream);
}

IHttpRequest::Pointer GoogleDrive::downloadFileRequest(const IItem& item,
                                                       std::ostream&) const {
  const Item& i = static_cast<const Item&>(item);
  if (isGoogleMimeType(i.mime_type())) {
    IHttpRequest::Pointer request = http()->create(
        endpoint() + "/drive/v3/files/" + item.id() + "/export", "GET");
    request->setParameter("mimeType", exported_mime_type(i.mime_type()));
    return request;
  } else {
    IHttpRequest::Pointer request =
        http()->create(endpoint() + "/drive/v3/files/" + item.id(), "GET");
    request->setParameter("alt", "media");
    return request;
  }
}

ICloudProvider::DownloadFileRequest::Pointer GoogleDrive::downloadFileAsync(
    IItem::Pointer file, IDownloadFileCallback::Pointer callback, Range range) {
  auto f = std::static_pointer_cast<Item>(file);
  if (isGoogleMimeType(f->mime_type()) && range != FullRange) {
    return std::make_shared<Request<EitherError<void>>>(
               shared_from_this(),
               [=](EitherError<void> e) { callback->done(e); },
               [](Request<EitherError<void>>::Pointer r) {
                 r->done(
                     Error{IHttpRequest::ServiceUnavailable,
                           "can't download sub-range of google specific file"});

               })
        ->run();
  }
  return std::make_shared<cloudstorage::DownloadFileRequest>(
             shared_from_this(), std::move(file), std::move(callback), range,
             std::bind(&CloudProvider::downloadFileRequest, this, _1, _2))
      ->run();
}

ICloudProvider::UploadFileRequest::Pointer GoogleDrive::uploadFileAsync(
    IItem::Pointer directory, const std::string& filename,
    IUploadFileCallback::Pointer cb) {
  auto resolve = [=](Request<EitherError<IItem>>::Pointer r) {
    r->subrequest(listDirectoryAsync(directory, [=](EitherError<
                                                    std::vector<IItem::Pointer>>
                                                        e) {
      if (e.left()) return r->done(e.left());
      IItem::Pointer item = nullptr;
      int cnt = 0;
      for (auto&& i : *e.right())
        if (i->filename() == filename) {
          item = i;
          cnt++;
        }
      auto stream_wrapper = std::make_shared<UploadStreamWrapper>(
          std::bind(&IUploadFileCallback::putData, cb.get(), _1, _2),
          cb->size());
      if (cnt != 1)
        return cloudstorage::UploadFileRequest::resolve(
            r, stream_wrapper, directory, filename, cb);
      r->send(
          [=](util::Output) {
            cb->reset();
            stream_wrapper->reset();
            return upload(*directory,
                          endpoint() + "/upload/drive/v3/files/" + item->id(),
                          "PATCH", filename, stream_wrapper->prefix_,
                          stream_wrapper->suffix_);
          },
          [=](EitherError<Response> e) {
            if (e.left()) return r->done(e.left());
            try {
              r->done(r->provider()->uploadFileResponse(*directory, filename,
                                                        stream_wrapper->size_,
                                                        e.right()->output()));
            } catch (const std::exception&) {
              r->done(Error{IHttpRequest::Failure, e.right()->output().str()});
            }
          },
          [=] { return std::make_shared<std::iostream>(stream_wrapper.get()); },
          std::make_shared<std::stringstream>(), nullptr,
          std::bind(&IUploadFileCallback::progress, cb, _1, _2), true);
    }));
  };
  return std::make_shared<Request<EitherError<IItem>>>(
             shared_from_this(), [=](EitherError<IItem> e) { cb->done(e); },
             resolve)
      ->run();
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
                        "mimeType,iconLink,parents,size,modifiedTime");
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
  request->setParameter("fields",
                        "id,name,thumbnailLink,trashed,"
                        "mimeType,iconLink,parents,size,modifiedTime");
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
  request->setParameter("fields",
                        "id,name,thumbnailLink,trashed,"
                        "mimeType,iconLink,parents,size,modifiedTime");
  Json::Value json;
  json["name"] = name;
  input << json;
  return request;
}

IItem::Pointer GoogleDrive::getItemDataResponse(std::istream& response) const {
  return toItem(util::json::from_stream(response));
}

std::vector<IItem::Pointer> GoogleDrive::listDirectoryResponse(
    const IItem& item, std::istream& stream,
    std::string& next_page_token) const {
  auto response = util::json::from_stream(stream);
  std::vector<IItem::Pointer> result;
  for (Json::Value v : response["files"]) result.push_back(toItem(v));
  if (item.id() == rootDirectory()->id())
    result.push_back(util::make_unique<Item>(
        "shared", "shared", IItem::UnknownSize, IItem::UnknownTimeStamp,
        IItem::FileType::Directory));

  if (response.isMember("nextPageToken"))
    next_page_token = response["nextPageToken"].asString();
  return result;
}

IHttpRequest::Pointer GoogleDrive::upload(const IItem& f,
                                          const std::string& url,
                                          const std::string& method,
                                          const std::string& filename,
                                          std::ostream& prefix_stream,
                                          std::ostream& suffix_stream) const {
  const std::string separator = "fWoDm9QNn3v3Bq3bScUX";
  const Item& item = static_cast<const Item&>(f);
  IHttpRequest::Pointer request = http()->create(url, method);
  request->setHeaderParameter("Content-Type",
                              "multipart/related; boundary=" + separator);
  request->setParameter("uploadType", "multipart");
  request->setParameter("fields",
                        "id,name,thumbnailLink,trashed,"
                        "mimeType,iconLink,parents,size,modifiedTime");
  Json::Value request_data;
  auto it = filename.find_last_of('.');
  if (it != std::string::npos) {
    auto mime = google_extension_to_mime_type(filename.substr(it));
    if (!mime.empty()) request_data["mimeType"] = mime;
  }
  if (method == "POST") {
    request_data["name"] = filename;
    request_data["parents"].append(item.id());
  }
  prefix_stream << "--" << separator << "\r\n"
                << "Content-Type: application/json; charset=UTF-8\r\n\r\n"
                << util::json::to_string(request_data) << "\r\n"
                << "--" << separator << "\r\n"
                << "Content-Type: \r\n\r\n";
  suffix_stream << "\r\n--" << separator << "--\r\n";
  return request;
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
  if (isGoogleMimeType(v["mimeType"].asString()) &&
      item->filename().find('.') == std::string::npos) {
    item->set_filename(item->filename() +
                       exported_extension(v["mimeType"].asString()));
  }
  item->set_hidden(v["trashed"].asBool());
  std::string thumnail_url = v["thumbnailLink"].asString();
  if (!thumnail_url.empty() && isGoogleMimeType(v["mimeType"].asString()))
    thumnail_url += "&access_token=" + access_token();
  else if (thumnail_url.empty())
    thumnail_url = v["iconLink"].asString();
  item->set_thumbnail_url(thumnail_url);
  item->set_mime_type(v["mimeType"].asString());
  std::vector<std::string> parents;
  for (auto id : v["parents"]) parents.push_back(id.asString());
  item->set_parents(parents);
  return std::move(item);
}

void GoogleDrive::Auth::initialize(IHttp* http, IHttpServerFactory* factory) {
  cloudstorage::Auth::initialize(http, factory);
  if (client_id().empty()) {
    set_client_id(
        "646432077068-hmvk44qgo6d0a64a5h9ieue34p3j2dcv.apps.googleusercontent."
        "com");
    set_client_secret("1f0FG5ch-kKOanTAv1Bqdp9U");
  }
}

std::string GoogleDrive::Auth::authorizeLibraryUrl() const {
  std::string response_type = "code";
  std::string scope;
  if (permission() == Permission::ReadMetaData)
    scope = "https://www.googleapis.com/auth/drive.metadata.readonly";
  else if (permission() == Permission::Read)
    scope = "https://www.googleapis.com/auth/drive.readonly";
  else
    scope = "https://www.googleapis.com/auth/drive";
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
  auto response = util::json::from_stream(data);
  Token::Pointer token = util::make_unique<Token>();
  token->token_ = response["access_token"].asString();
  token->refresh_token_ = response["refresh_token"].asString();
  token->expires_in_ = response["expires_in"].asInt();
  return token;
}

IAuth::Token::Pointer GoogleDrive::Auth::refreshTokenResponse(
    std::istream& data) const {
  auto response = util::json::from_stream(data);
  Token::Pointer token = util::make_unique<Token>();
  token->token_ = response["access_token"].asString();
  token->refresh_token_ = access_token()->refresh_token_;
  token->expires_in_ = response["expires_in"].asInt();
  return token;
}

}  // namespace cloudstorage
