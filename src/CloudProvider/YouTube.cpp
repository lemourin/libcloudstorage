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
#include "Request/ListDirectoryPageRequest.h"
#include "Request/ListDirectoryRequest.h"
#include "Request/Request.h"

#include "Utility/Item.h"
#include "Utility/Utility.h"

const std::string AUDIO_DIRECTORY = "audio";
const std::string AUDIO_DIRECTORY_ID =
    "eyJhdWRpbyI6dHJ1ZSwicGxheWxpc3QiOnRydWUsImlkIjoiYXVkaW8ifQo=";

using namespace std::placeholders;

namespace cloudstorage {

namespace {

struct YouTubeItem {
  bool audio;
  bool playlist;
  std::string id;
};

YouTubeItem from_string(const std::string& id) {
  Json::Value json;
  if (Json::Reader().parse(util::from_base64(id), json)) {
    return {json["audio"].asBool(), json["playlist"].asBool(),
            json["id"].asString()};
  } else {
    return {};
  }
}

std::string to_string(YouTubeItem item) {
  Json::Value json;
  json["audio"] = item.audio;
  json["playlist"] = item.playlist;
  json["id"] = item.id;
  return util::to_base64(Json::FastWriter().write(json));
}

}  // namespace

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

ICloudProvider::ListDirectoryPageRequest::Pointer
YouTube::listDirectoryPageAsync(IItem::Pointer directory,
                                const std::string& token,
                                ListDirectoryPageCallback complete) {
  return std::make_shared<cloudstorage::ListDirectoryPageRequest>(
             shared_from_this(), directory, token, complete,
             [](int code) { return code == IHttpRequest::NotFound; })
      ->run();
}

ICloudProvider::ListDirectoryRequest::Pointer YouTube::listDirectoryAsync(
    IItem::Pointer item, IListDirectoryCallback::Pointer callback) {
  return std::make_shared<cloudstorage::ListDirectoryRequest>(
             shared_from_this(), std::move(item), std::move(callback),
             [](int code) { return code == IHttpRequest::NotFound; })
      ->run();
}

ICloudProvider::GetItemDataRequest::Pointer YouTube::getItemDataAsync(
    const std::string& id, GetItemDataCallback callback) {
  auto r = std::make_shared<Request<EitherError<IItem>>>(shared_from_this());
  r->set(
      [=](Request<EitherError<IItem>>::Pointer r) {
        if (id == rootDirectory()->id()) return r->done(rootDirectory());
        if (id == AUDIO_DIRECTORY_ID) {
          IItem::Pointer i = std::make_shared<Item>(
              AUDIO_DIRECTORY, AUDIO_DIRECTORY_ID, IItem::UnknownSize,
              IItem::UnknownTimeStamp, IItem::FileType::Directory);
          return r->done(i);
        }
        auto response_stream = std::make_shared<std::stringstream>();
        r->sendRequest(
            [=](util::Output input) { return getItemDataRequest(id, *input); },
            [=](EitherError<util::Output> e) {
              if (e.left()) return r->done(e.left());
              auto id_data = from_string(id);
              auto item = getItemDataResponse(*response_stream, id_data.audio);
              auto stream = std::make_shared<std::stringstream>();
              auto request = [=](std::string format) {
                return [=](util::Output) {
                  auto request =
                      http()->create(youtube_dl_url_ + "/api/info", "GET");
                  request->setParameter("format", format);
                  request->setParameter(
                      "url", "http://youtube.com/watch?v=" + id_data.id);
                  return request;
                };
              };
              auto response = [=](EitherError<util::Output> e) {
                if (e.left()) return r->done(e.left());
                try {
                  Json::Value response;
                  *stream >> response;
                  for (auto v : response["info"]["formats"]) {
                    if (v["format_id"] == response["info"]["format_id"]) {
                      auto nitem = static_cast<Item*>(item.get());
                      auto size = v["filesize"].isNull()
                                      ? IItem::UnknownSize
                                      : v["filesize"].asUInt64();
                      nitem->set_size(size);
                      nitem->set_filename(item->filename() + "." +
                                          v["ext"].asString());
                      nitem->set_url(v["url"].asString());
                    }
                  }
                  if (item->size() == IItem::UnknownSize) {
                    auto request = http()->create(item->url(), "HEAD");
                    auto input = std::make_shared<std::stringstream>();
                    auto output = std::make_shared<std::stringstream>();
                    auto error = std::make_shared<std::stringstream>();
                    r->send(request.get(),
                            [=](IHttpRequest::Response response) {
                              if (IHttpRequest::isSuccess(response.http_code_))
                                static_cast<Item*>(item.get())
                                    ->set_size(response.content_length_);
                              r->done(item);
                            },
                            input, output, error);
                  } else {
                    r->done(item);
                  }
                } catch (std::exception) {
                  r->done(Error{IHttpRequest::Failure, stream->str()});
                }
              };
              if (item->type() == IItem::FileType::Audio)
                r->sendRequest(request("bestaudio"), response, stream);
              else if (item->type() == IItem::FileType::Video)
                r->sendRequest(request("best"), response, stream);
              else {
                r->done(item);
              }
            },
            response_stream);
      },
      callback);
  return r->run();
}

ICloudProvider::DownloadFileRequest::Pointer YouTube::downloadFileAsync(
    IItem::Pointer item, IDownloadFileCallback::Pointer cb, Range range) {
  auto r = std::make_shared<Request<EitherError<void>>>(shared_from_this());
  auto callback = cb.get();
  r->set(
      [=](Request<EitherError<void>>::Pointer r) {
        std::string url = item->url();
        auto download = [=](std::string url) {
          auto wrapper = std::make_shared<DownloadStreamWrapper>(std::bind(
              &IDownloadFileCallback::receivedData, callback, _1, _2));
          auto stream = std::make_shared<std::ostream>(wrapper.get());
          r->sendRequest(
              [=](util::Output) {
                auto r = http()->create(url, "GET");
                if (range != FullRange)
                  r->setHeaderParameter("Range", util::range_to_string(range));
                return r;
              },
              [=](EitherError<util::Output> e) {
                (void)wrapper;
                if (e.left())
                  r->done(e.left());
                else
                  r->done(nullptr);
              },
              stream,
              std::bind(&IDownloadFileCallback::progress, callback, _1, _2));
        };
        if (item->type() == IItem::FileType::Audio) {
          r->subrequest(getItemDataAsync(item->id(), [=](EitherError<IItem> e) {
            if (e.left())
              r->done(e.left());
            else
              download(e.right()->url());
          }));
        } else {
          download(item->url());
        }
      },
      [=](EitherError<void> e) { cb->done(e); });
  return r->run();
}

IHttpRequest::Pointer YouTube::getItemDataRequest(const std::string& full_id,
                                                  std::ostream&) const {
  auto id_data = from_string(full_id);
  if (!id_data.playlist) {
    auto request = http()->create(endpoint() + "/youtube/v3/videos", "GET");
    request->setParameter("part", "contentDetails,snippet");
    request->setParameter("id", id_data.id);
    return request;
  } else {
    auto request = http()->create(endpoint() + "/youtube/v3/playlists", "GET");
    request->setParameter("part", "contentDetails,snippet");
    request->setParameter("id", id_data.id);
    return request;
  }
}

IHttpRequest::Pointer YouTube::listDirectoryRequest(
    const IItem& item, const std::string& page_token, std::ostream&) const {
  if (item.id() == rootDirectory()->id() || item.id() == AUDIO_DIRECTORY_ID) {
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
    request->setParameter("playlistId", from_string(item.id()).id);
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
    const IItem& directory, std::istream& stream,
    std::string& next_page_token) const {
  Json::Value response;
  Json::Reader().parse(static_cast<const std::stringstream&>(std::stringstream()
                                                             << stream.rdbuf())
                           .str(),
                       response);
  std::vector<IItem::Pointer> result;
  bool audio = from_string(directory.id()).audio;
  std::string name_prefix = !audio ? "" : AUDIO_DIRECTORY + " ";
  if (response["kind"].asString() == "youtube#channelListResponse") {
    Json::Value related_playlists =
        response["items"][0]["contentDetails"]["relatedPlaylists"];
    for (const std::string& name : related_playlists.getMemberNames()) {
      auto item = util::make_unique<Item>(
          name_prefix + name,
          to_string({audio, true, related_playlists[name].asString()}),
          IItem::UnknownSize, IItem::UnknownTimeStamp,
          IItem::FileType::Directory);
      item->set_thumbnail_url(
          response["items"][0]["snippet"]["thumbnails"]["default"]["url"]
              .asString());
      result.push_back(std::move(item));
    }
    next_page_token = "real_playlist";
  } else {
    for (const Json::Value& v : response["items"])
      result.push_back(toItem(v, response["kind"].asString(), audio));
  }
  if (response.isMember("nextPageToken"))
    next_page_token = response["nextPageToken"].asString();
  else if (directory.id() == rootDirectory()->id() && next_page_token.empty())
    result.push_back(util::make_unique<Item>(
        AUDIO_DIRECTORY, AUDIO_DIRECTORY_ID, IItem::UnknownSize,
        IItem::UnknownTimeStamp, IItem::FileType::Directory));
  return result;
}

IItem::Pointer YouTube::toItem(const Json::Value& v, std::string kind,
                               bool audio) const {
  std::string name_prefix = audio ? AUDIO_DIRECTORY + " " : "";
  if (kind == "youtube#playlistListResponse") {
    auto item = util::make_unique<Item>(
        name_prefix + v["snippet"]["title"].asString(),
        to_string({audio, true, v["id"].asString()}), IItem::UnknownSize,
        IItem::UnknownTimeStamp, IItem::FileType::Directory);
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
      throw std::logic_error("invalid kind");

    auto item = util::make_unique<Item>(
        v["snippet"]["title"].asString() + (audio ? ".webm" : ".mp4"),
        to_string({audio, false, video_id}), IItem::UnknownSize,
        IItem::UnknownTimeStamp,
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

std::string YouTube::Auth::authorizeLibraryUrl() const {
  return "https://accounts.google.com/o/oauth2/auth?client_id=" + client_id() +
         "&redirect_uri=" + redirect_uri() +
         "&scope=https://www.googleapis.com/auth/youtube"
         "&response_type=code&access_type=offline&prompt=consent"
         "&state=" +
         state();
}

}  // namespace cloudstorage
