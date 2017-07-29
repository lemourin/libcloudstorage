/*****************************************************************************
 * YouTube.cpp : YouTube implementation
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

#include "YouTube.h"

#include <json/json.h>
#include <cstring>

#include "Request/DownloadFileRequest.h"
#include "Request/Request.h"
#include "Utility/Item.h"
#include "Utility/Utility.h"

const std::string VIDEO_ID_PREFIX = "video###";
const std::string AUDIO_ID_PREFIX = "audio###";
const std::string AUDIO_DIRECTORY = "audio";

using namespace std::placeholders;

namespace cloudstorage {

YouTube::YouTube()
    : CloudProvider(util::make_unique<Auth>()),
      youtube_dl_url_("http://youtube-dl.appspot.com") {}

void YouTube::initialize(InitData&& data) {
  setWithHint(data.hints_, "youtube_dl_url",
              [this](std::string url) { youtube_dl_url_ = url; });
  CloudProvider::initialize(std::move(data));
}

ICloudProvider::Hints YouTube::hints() const {
  Hints result = {{"youtube_dl_url", youtube_dl_url_}};
  auto t = CloudProvider::hints();
  result.insert(t.begin(), t.end());
  return result;
}

std::string YouTube::name() const { return "youtube"; }

std::string YouTube::endpoint() const { return "https://www.googleapis.com"; }

ICloudProvider::ListDirectoryRequest::Pointer YouTube::listDirectoryAsync(
    IItem::Pointer item, IListDirectoryCallback::Pointer callback) {
  auto r = util::make_unique<Request<EitherError<std::vector<IItem::Pointer>>>>(
      shared_from_this());
  auto is_fine = [](int code) { return code == 404; };
  r->set_error_callback(
      [callback, is_fine](Request<EitherError<std::vector<IItem::Pointer>>>* r,
                          int code, const std::string& error) {
        if (!is_fine(code)) callback->error(r->error_string(code, error));
      });
  r->set_resolver([item, callback, is_fine,
                   this](Request<EitherError<std::vector<IItem::Pointer>>>* r)
                      -> EitherError<std::vector<IItem::Pointer>> {
    if (item->type() != IItem::FileType::Directory) {
      callback->error("Trying to list non-directory.");
      return Error{403, "trying to list non-directory"};
    }
    std::string page_token;
    std::vector<IItem::Pointer> result;
    bool failure = false;
    do {
      std::stringstream output_stream;
      int code = r->sendRequest(
          [this, item, &page_token](std::ostream& i) {
            return listDirectoryRequest(*item, page_token, i);
          },
          output_stream);
      if (IHttpRequest::isSuccess(code) || is_fine(code)) {
        page_token = "";
        if (is_fine(code)) output_stream << "{}";
        for (auto& t : listDirectoryResponse(item, output_stream, page_token)) {
          callback->receivedItem(t);
          result.push_back(t);
        }
      } else
        failure = true;
    } while (!page_token.empty() && !failure);
    if (!failure) callback->done(result);
    return result;
  });
  return std::move(r);
}

ICloudProvider::GetItemDataRequest::Pointer YouTube::getItemDataAsync(
    const std::string& id, GetItemDataCallback callback) {
  auto r = util::make_unique<Request<EitherError<IItem>>>(shared_from_this());
  r->set_resolver([this, id, callback](
                      Request<EitherError<IItem>>* r) -> EitherError<IItem> {
    if (id == AUDIO_DIRECTORY) {
      IItem::Pointer r = util::make_unique<Item>(
          AUDIO_DIRECTORY, AUDIO_DIRECTORY, IItem::FileType::Directory);
      callback(r);
      return r;
    }
    std::stringstream response_stream;
    Error error;
    int code = r->sendRequest(
        [this, id](std::ostream& input) {
          return getItemDataRequest(id, input);
        },
        response_stream, &error);
    if (IHttpRequest::isSuccess(code)) {
      auto i = getItemDataResponse(
          response_stream, id.find(AUDIO_ID_PREFIX) != std::string::npos);
      if (i->type() == IItem::FileType::Audio) {
        code = r->sendRequest(
            [this, id](std::ostream&) {
              auto request =
                  http()->create(youtube_dl_url_ + "/api/info", "GET");
              request->setParameter("format", "bestaudio");
              request->setParameter(
                  "url",
                  "http://youtube.com/watch?v=" +
                      extractId(id).substr(VIDEO_ID_PREFIX.length()));
              return request;
            },
            response_stream, &error);
        if (IHttpRequest::isSuccess(code)) {
          Json::Value response;
          response_stream >> response;
          for (const Json::Value& v : response["info"]["formats"])
            if (v["format_id"] == response["info"]["format_id"]) {
              auto item = util::make_unique<Item>(
                  i->filename() + "." + v["ext"].asString(), i->id(),
                  i->type());
              item->set_url(v["url"].asString());
              i = std::move(item);
            }
        }
      }
      callback(i);
      return i;
    }
    callback(error);
    return error;
  });
  return std::move(r);
}

ICloudProvider::DownloadFileRequest::Pointer YouTube::downloadFileAsync(
    IItem::Pointer item, IDownloadFileCallback::Pointer callback) {
  auto r = util::make_unique<Request<EitherError<void>>>(shared_from_this());
  r->set_error_callback([this, callback](Request<EitherError<void>>* r,
                                         int code, const std::string& desc) {
    callback->error(r->error_string(code, desc));
  });
  r->set_resolver([this, item, callback](
                      Request<EitherError<void>>* r) -> EitherError<void> {
    std::string url = item->url();
    if (item->type() == IItem::FileType::Audio) {
      Request<EitherError<void>>::Semaphore semaphore(r);
      auto t = getItemDataAsync(
          item->id(), [&semaphore](EitherError<IItem>) { semaphore.notify(); });
      semaphore.wait();
      if (r->is_cancelled())
        t->cancel();
      else if (t->result().right())
        url = t->result().right()->url();
    }
    DownloadStreamWrapper wrapper(std::bind(
        &IDownloadFileCallback::receivedData, callback.get(), _1, _2));
    std::ostream stream(&wrapper);
    Error error;
    int code = r->sendRequest(
        [this, url](std::ostream&) { return http()->create(url, "GET"); },
        stream, &error,
        std::bind(&IDownloadFileCallback::progress, callback.get(), _1, _2));
    if (IHttpRequest::isSuccess(code)) {
      callback->done();
      return nullptr;
    } else {
      return error;
    }
  });
  return std::move(r);
}

IHttpRequest::Pointer YouTube::getItemDataRequest(const std::string& full_id,
                                                  std::ostream&) const {
  std::string id = extractId(full_id);
  if (id.find(VIDEO_ID_PREFIX) != std::string::npos) {
    auto request = http()->create(endpoint() + "/youtube/v3/videos", "GET");
    request->setParameter("part", "contentDetails,snippet");
    request->setParameter("id", id.substr(VIDEO_ID_PREFIX.length()));
    return request;
  } else {
    auto request = http()->create(endpoint() + "/youtube/v3/playlists", "GET");
    request->setParameter("part", "contentDetails,snippet");
    request->setParameter("id", id);
    return request;
  }
}

IHttpRequest::Pointer YouTube::listDirectoryRequest(
    const IItem& item, const std::string& page_token, std::ostream&) const {
  if (item.id() == rootDirectory()->id() || item.id() == AUDIO_DIRECTORY) {
    if (page_token.empty())
      return http()->create(
          endpoint() +
              "/youtube/v3/"
              "channels?mine=true&part=contentDetails,snippet",
          "GET");
    else if (page_token == "real_playlist")
      return http()->create(
          endpoint() +
              "/youtube/v3/"
              "playlists?mine=true&maxResults=50&part=snippet",
          "GET");
    else
      return http()->create(
          endpoint() +
              "/youtube/v3/"
              "playlists?mine=true&maxResults=50&part=snippet&pageToken=" +
              page_token,
          "GET");
  } else {
    auto request =
        http()->create(endpoint() + "/youtube/v3/playlistItems", "GET");
    request->setParameter("part", "snippet");
    request->setParameter("maxResults", "50");
    request->setParameter("playlistId", extractId(item.id()));
    if (!page_token.empty()) request->setParameter("pageToken", page_token);
    return request;
  }
}

IItem::Pointer YouTube::getItemDataResponse(std::istream& stream,
                                            bool audio) const {
  Json::Value response;
  stream >> response;
  return toItem(response["items"][0], response["kind"].asString(), audio);
}

std::vector<IItem::Pointer> YouTube::listDirectoryResponse(
    IItem::Pointer directory, std::istream& stream,
    std::string& next_page_token) const {
  Json::Value response;
  stream >> response;
  std::vector<IItem::Pointer> result;
  std::string id_prefix =
      directory->id().find(AUDIO_ID_PREFIX) != std::string::npos ||
              directory->id() == AUDIO_DIRECTORY
          ? AUDIO_ID_PREFIX
          : "";
  std::string name_prefix = id_prefix.empty() ? "" : AUDIO_DIRECTORY + " ";
  if (response["kind"].asString() == "youtube#channelListResponse") {
    Json::Value related_playlists =
        response["items"][0]["contentDetails"]["relatedPlaylists"];
    for (const std::string& name : related_playlists.getMemberNames()) {
      auto item = util::make_unique<Item>(
          name_prefix + name, id_prefix + related_playlists[name].asString(),
          IItem::FileType::Directory);
      item->set_thumbnail_url(
          response["items"][0]["snippet"]["thumbnails"]["default"]["url"]
              .asString());
      result.push_back(std::move(item));
    }
    next_page_token = "real_playlist";
  } else {
    for (const Json::Value& v : response["items"])
      result.push_back(
          toItem(v, response["kind"].asString(), !id_prefix.empty()));
  }
  if (response.isMember("nextPageToken"))
    next_page_token = response["nextPageToken"].asString();
  else if (directory->id() == rootDirectory()->id() && next_page_token.empty())
    result.push_back(util::make_unique<Item>(AUDIO_DIRECTORY, AUDIO_DIRECTORY,
                                             IItem::FileType::Directory));
  return result;
}

IItem::Pointer YouTube::toItem(const Json::Value& v, std::string kind,
                               bool audio) const {
  std::string id_prefix = audio ? AUDIO_ID_PREFIX : "";
  std::string name_prefix = audio ? AUDIO_DIRECTORY + " " : "";
  if (kind == "youtube#playlistListResponse") {
    auto item = util::make_unique<Item>(
        name_prefix + v["snippet"]["title"].asString(),
        id_prefix + v["id"].asString(), IItem::FileType::Directory);
    item->set_thumbnail_url(
        v["snippet"]["thumbnails"]["default"]["url"].asString());
    return std::move(item);
  } else {
    std::string video_id;
    if (kind == "youtube#playlistItemListResponse")
      video_id = v["snippet"]["resourceId"]["videoId"].asString();
    else if (kind == "youtube#videoListResponse")
      video_id = v["id"].asString();
    else
      return nullptr;
    auto item = util::make_unique<Item>(
        v["snippet"]["title"].asString() + (audio ? ".webm" : ".mp4"),
        id_prefix + VIDEO_ID_PREFIX + video_id,
        audio ? IItem::FileType::Audio : IItem::FileType::Video);
    item->set_thumbnail_url(
        v["snippet"]["thumbnails"]["default"]["url"].asString());
    item->set_url(youtube_dl_url_ +
                  "/api/play?url=https://www.youtube.com/"
                  "watch?v=" +
                  video_id);
    return std::move(item);
  }
}

std::string YouTube::extractId(const std::string& full_id) const {
  if (full_id.find(AUDIO_ID_PREFIX) != std::string::npos)
    return full_id.substr(AUDIO_ID_PREFIX.length());
  else
    return full_id;
}

std::string YouTube::Auth::authorizeLibraryUrl() const {
  return "https://accounts.google.com/o/oauth2/auth?client_id=" + client_id() +
         "&redirect_uri=" + redirect_uri() +
         "&scope=https://www.googleapis.com/auth/youtube"
         "&response_type=code&access_type=offline&prompt=consent"
         "&state=" +
         state();
}

}  // namespace cloudstorage
