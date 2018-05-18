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
#include <algorithm>
#include <cstring>

#include "Request/DownloadFileRequest.h"
#include "Request/ListDirectoryPageRequest.h"
#include "Request/ListDirectoryRequest.h"
#include "Request/Request.h"

#include "Utility/Item.h"
#include "Utility/Utility.h"

#undef CreateDirectory

const std::string AUDIO_DIRECTORY = "Audio";
const std::string AUDIO_DIRECTORY_ID = cloudstorage::util::to_base64(
    R"({"type":3,"id":"audio"})");

const std::string HIGH_QUALITY_DIRECTORY = "High Quality";
const std::string HIGH_QUALITY_DIRECTORY_ID = cloudstorage::util::to_base64(
    R"({"type":6,"id":"high quality"})");

const std::string LIKED_VIDEOS = "Liked videos";
const std::string UPLOADED_VIDEOS = "Uploaded videos";

using namespace std::placeholders;

namespace cloudstorage {

namespace {

struct YouTubeItem {
  enum Type {
    Audio = 1 << 0,
    Playlist = 1 << 1,
    HighQuality = 1 << 2,
    RelatedPlaylist = 1 << 3,
    DontMerge = 1 << 4
  };
  int type = 0;
  std::string id;
  std::string video_id;
};

struct VideoInfo {
  std::string scrambled_signature;
  std::string signature;
  std::string type;
  std::string url;
  int bitrate;
  std::string codec;
  std::string quality_label;
  std::string init_range;
  std::string index_range;
  uint64_t size = 0;
};

YouTubeItem from_string(const std::string& id) {
  try {
    auto json =
        util::json::from_stream(std::stringstream(util::from_base64(id)));
    return {json["type"].asInt(), json["id"].asString(),
            json["video_id"].asString()};
  } catch (const Json::Exception&) {
    return {};
  }
}

std::string to_string(YouTubeItem item) {
  Json::Value json;
  json["type"] = item.type;
  json["id"] = item.id;
  json["video_id"] = item.video_id;
  return util::to_base64(util::json::to_string(json));
}

std::string related_playlist_name(const std::string& name) {
  if (name == "likes")
    return LIKED_VIDEOS;
  else if (name == "uploads")
    return UPLOADED_VIDEOS;
  else
    return name;
}

std::string xml_escape(const std::string& url) {
  std::string result;
  for (auto c : url)
    if (c == '&')
      result += "&amp;";
    else
      result += c;
  return result;
}

VideoInfo video_info(const std::string& url) {
  std::stringstream ss(url);
  std::string token;
  VideoInfo result = {};
  while (std::getline(ss, token, '&')) {
    std::stringstream stream(token);
    std::string key, value;
    std::getline(stream, key, '=');
    std::getline(stream, value, '=');
    if (key == "s")
      result.scrambled_signature = value;
    else if (key == "sig")
      result.signature = value;
    else if (key == "type") {
      result.type = util::Url::unescape(value);
      auto text = "codecs=\"";
      auto it = result.type.find(text) + strlen(text);
      auto it_end = result.type.find("\"", it);
      result.codec =
          std::string(result.type.begin() + it, result.type.begin() + it_end);
    } else if (key == "url")
      result.url = util::Url::unescape(value);
    else if (key == "bitrate")
      result.bitrate = std::stoi(value);
    else if (key == "quality_label")
      result.quality_label = value;
    else if (key == "init")
      result.init_range = value;
    else if (key == "index")
      result.index_range = value;
    else if (key == "clen")
      result.size = std::stoull(value);
  }
  return result;
}

std::string combine(const VideoInfo& video, const VideoInfo& audio) {
  return "cloudstorage://youtube/?video=" + util::Url::escape(video.url) +
         "&audio=" + util::Url::escape(audio.url) +
         "&video_codec=" + video.codec +
         "&video_bitrate=" + std::to_string(video.bitrate) +
         "&video_init_range=" + video.init_range +
         "&video_index_range=" + video.index_range +
         "&video_size=" + std::to_string(video.size) +
         "&audio_codec=" + audio.codec +
         "&audio_bitrate=" + std::to_string(audio.bitrate) +
         "&audio_init_range=" + audio.init_range +
         "&audio_index_range=" + audio.index_range +
         "&audio_size=" + std::to_string(audio.size);
}

EitherError<std::string> descramble(const std::string& scrambled,
                                    std::stringstream& stream) {
  auto find_descrambler = [](std::stringstream& stream) {
    auto player = stream.str();
    const std::string descrambler_search = "\"signature\":\"sig\"";
    auto it = player.find(descrambler_search) + 3;
    if (it == std::string::npos)
      throw std::logic_error(util::Error::COULD_NOT_FIND_DESCRAMBLER_NAME);
    stream.seekg(it + descrambler_search.length());
    std::string descrambler;
    std::getline(stream, descrambler, '(');
    return descrambler;
  };
  auto find_helper = [](const std::string& code, std::stringstream& stream) {
    auto player = stream.str();
    auto helper = code.substr(0, code.find_first_of('.'));
    const std::string helper_search = "var " + helper + "={";
    auto it = player.find(helper_search);
    if (it == std::string::npos)
      throw std::logic_error(util::Error::COULD_NOT_FIND_HELPER_FUNCTIONS);
    stream.seekg(it + helper_search.length());
    std::string result;
    int cnt = 1;
    char c;
    while (cnt != 0 && stream.read(&c, 1)) {
      if (c == '{')
        cnt++;
      else if (c == '}')
        cnt--;
      if (cnt != 0) result += c;
    }
    return result;
  };
  auto transformations = [](const std::string& helper) {
    std::stringstream stream(helper);
    std::unordered_map<std::string, std::string> result;
    while (stream) {
      std::string key, value;
      stream >> std::ws;
      std::getline(stream, key, ':');
      std::getline(stream, value, '}');
      if (stream) result[key] = value;
      std::string dummy;
      std::getline(stream, dummy, ',');
    }
    return result;
  };
  auto find_descrambler_code = [](const std::string& name,
                                  std::stringstream& stream) {
    auto player = stream.str();
    const std::string function_search = name + "=function(a){";
    auto it = player.find(function_search);
    if (it == std::string::npos)
      throw std::logic_error(
          util::Error::COULD_NOT_FIND_DESCRABMLER_DEFINITION);
    stream.seekg(it + function_search.length());
    std::string code;
    std::getline(stream, code, ';');
    std::getline(stream, code, '}');
    return code.substr(0, code.find_last_of(';') + 1);
  };
  auto transform = [](std::string code, const std::string& function,
                      const std::unordered_map<std::string, std::string>& t) {
    std::stringstream stream(function);
    std::string operation;
    while (std::getline(stream, operation, ';')) {
      std::stringstream stream(operation);
      std::string func_name, arg, dummy;
      std::getline(stream, dummy, '.');
      std::getline(stream, func_name, '(');
      std::getline(stream, dummy, ',');
      std::getline(stream, arg, ')');
      auto func = t.find(func_name);
      if (func == t.end())
        throw std::logic_error(util::Error::INVALID_TRANSFORMATION_FUNCTION);
      auto value = std::stoull(arg);
      if (func->second.find("splice") != std::string::npos)
        code.erase(0, value);
      else if (func->second.find("reverse") != std::string::npos)
        std::reverse(code.begin(), code.end());
      else if (func->second.find("a[0]=a[b%a.length];") != std::string::npos)
        std::swap(code[0], code[value % code.length()]);
      else
        throw std::logic_error(util::Error::UNKNOWN_TRANSFORMATION);
    }
    return code;
  };
  try {
    auto descrambler = find_descrambler(stream);
    auto function = find_descrambler_code(descrambler, stream);
    return transform(scrambled, function,
                     transformations(find_helper(function, stream)));
  } catch (const std::logic_error& e) {
    return Error{IHttpRequest::Failure, e.what()};
  }
}

template <class Result>
void get_stream(typename Request<Result>::Pointer r, IItem::Pointer item,
                std::function<void(EitherError<std::string>)> complete) {
  auto get_config = [](std::stringstream& stream) {
    std::string page = stream.str();
    std::string player_str = "ytplayer.config = ";
    auto it = page.find(player_str);
    if (it == std::string::npos)
      throw std::logic_error(util::Error::YOUTUBE_CONFIG_NOT_FOUND);
    stream.seekg(it + player_str.length());
    return util::json::from_stream(stream);
  };
  auto get_url = [=](typename Request<Result>::Pointer r, Json::Value json) {
    auto data = from_string(item->id());
    std::stringstream stream(
        item->type() == IItem::FileType::Audio ||
                (data.type & YouTubeItem::HighQuality)
            ? json["args"]["adaptive_fmts"].asString()
            : json["args"]["url_encoded_fmt_stream_map"].asString());
    std::string url;
    VideoInfo best_audio, best_video;
    best_audio.bitrate = best_video.bitrate = -1;
    while (std::getline(stream, url, ',')) {
      auto d = video_info(url);
      if (d.type.find("video/mp4") != std::string::npos &&
          d.bitrate > best_video.bitrate)
        best_video = d;
      else if (d.type.find("audio/mp4") != std::string::npos &&
               d.bitrate > best_audio.bitrate)
        best_audio = d;
    }
    if (!(data.type & (YouTubeItem::Audio | YouTubeItem::HighQuality)))
      best_audio = best_video;
    if (best_video.url.empty())
      throw std::logic_error(util::Error::VIDEO_URL_NOT_FOUND);
    if (best_audio.url.empty())
      throw std::logic_error(util::Error::AUDIO_URL_NOT_FOUND);
    if (best_video.scrambled_signature.empty() &&
        best_audio.scrambled_signature.empty()) {
      if (!(data.type & YouTubeItem::HighQuality) ||
          (data.type & YouTubeItem::DontMerge))
        return complete((data.type & YouTubeItem::Audio) ? best_audio.url
                                                         : best_video.url);
      else
        return complete(combine(best_video, best_audio));
    }
    r->send(
        [=](util::Output) {
          return r->provider()->http()->create("http://youtube.com" +
                                               json["assets"]["js"].asString());
        },
        [=](EitherError<Response> e) {
          if (e.left()) return r->done(e.left());
          auto signature_video =
              descramble(best_video.scrambled_signature, e.right()->output());
          if (signature_video.left()) return r->done(signature_video.left());
          auto signature_audio =
              descramble(best_audio.scrambled_signature, e.right()->output());
          if (signature_audio.left()) return r->done(signature_audio.left());
          auto video_url =
              best_video.url + "&signature=" + *signature_video.right();
          auto audio_url =
              best_audio.url + "&signature=" + *signature_audio.right();
          if (!(data.type & YouTubeItem::HighQuality) ||
              (data.type & YouTubeItem::DontMerge)) {
            return complete((data.type & YouTubeItem::Audio) ? audio_url
                                                             : video_url);
          } else {
            auto nvideo = best_video;
            nvideo.url = video_url;
            auto naudio = best_audio;
            naudio.url = audio_url;
            return complete(combine(nvideo, naudio));
          }
        });
  };
  r->send(
      [=](util::Output) {
        return r->provider()->http()->create("http://youtube.com/watch?v=" +
                                             from_string(item->id()).video_id);
      },
      [=](EitherError<Response> e) {
        if (e.left()) return r->done(e.left());
        try {
          auto json = get_config(e.right()->output());
          get_url(r, json);
        } catch (const std::exception& e) {
          r->done(Error{IHttpRequest::Failure, e.what()});
        }
      });
}

std::string generate_dash_manifest(const std::string& data,
                                   const std::string& duration,
                                   const std::string& audio,
                                   const std::string& video) {
  auto form = util::parse_form(data);
  std::stringstream stream;
  stream << R"(<?xml version="1.0" encoding="UTF-8"?>
  <MPD xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
    xmlns="urn:mpeg:dash:schema:mpd:2011"
    xsi:schemaLocation="urn:mpeg:dash:schema:mpd:2011 DASH-MPD.xsd"
    type="static"
    mediaPresentationDuration="PT)"
         << duration << R"(S"
    minBufferTime="PT2S"
    profiles="urn:mpeg:dash:profile:isoff-main:2011">
    <Period id="0">
      <AdaptationSet mimeType="audio/mp4" codecs=")"
         << form["audio_codec"] <<
      R"(" contentType="audio" subsegmentAlignment="true" subsegmentStartsWithSAP="1">
        <Representation id="1" bandwidth=")"
         << form["audio_bitrate"] << R"(">
          <BaseURL>)"
         << xml_escape(audio) << R"(</BaseURL>
          <SegmentBase indexRange=")"
         << form["audio_index_range"] << R"(">
            <Initialization range=")"
         << form["audio_init_range"] << R"("/>
          </SegmentBase>
        </Representation>
      </AdaptationSet>
      <AdaptationSet mimeType="video/mp4" codecs=")"
         << form["video_codec"] <<
      R"(" contentType="video" subsegmentAlignment="true" subsegmentStartsWithSAP="1">
        <Representation id="2" bandwidth=")"
         << form["video_bitrate"] + R"(">
          <BaseURL>)"
         << xml_escape(video) << R"(</BaseURL>
          <SegmentBase indexRange=")"
         << form["video_index_range"] << R"(">
            <Initialization range=")"
         << form["video_init_range"] << R"("/>
          </SegmentBase>
        </Representation>
      </AdaptationSet>
    </Period>
  </MPD>)";
  return stream.str();
}

}  // namespace

YouTube::YouTube() : CloudProvider(util::make_unique<Auth>()) {}

IItem::Pointer YouTube::rootDirectory() const {
  return util::make_unique<Item>("/", util::to_base64("{}"), IItem::UnknownSize,
                                 IItem::UnknownTimeStamp,
                                 IItem::FileType::Directory);
}

std::string YouTube::name() const { return "youtube"; }

std::string YouTube::endpoint() const { return "https://www.googleapis.com"; }

ICloudProvider::OperationSet YouTube::supportedOperations() const {
  return GetItem | ListDirectoryPage | ListDirectory | DownloadFile |
         DeleteItem | GetItemUrl | RenameItem | CreateDirectory;
}

bool YouTube::isSuccess(int code,
                        const IHttpRequest::HeaderParameters& h) const {
  return CloudProvider::isSuccess(code, h) || code == IHttpRequest::NotFound;
}

ICloudProvider::GetItemDataRequest::Pointer YouTube::getItemDataAsync(
    const std::string& id, GetItemDataCallback callback) {
  return std::make_shared<Request<EitherError<IItem>>>(
             shared_from_this(), callback,
             [=](Request<EitherError<IItem>>::Pointer r) {
               if (id == rootDirectory()->id()) return r->done(rootDirectory());
               if (id == AUDIO_DIRECTORY_ID) {
                 IItem::Pointer i = std::make_shared<Item>(
                     AUDIO_DIRECTORY, AUDIO_DIRECTORY_ID, IItem::UnknownSize,
                     IItem::UnknownTimeStamp, IItem::FileType::Directory);
                 return r->done(i);
               }
               if (id == HIGH_QUALITY_DIRECTORY_ID) {
                 IItem::Pointer i = std::make_shared<Item>(
                     HIGH_QUALITY_DIRECTORY, HIGH_QUALITY_DIRECTORY_ID,
                     IItem::UnknownSize, IItem::UnknownTimeStamp,
                     IItem::FileType::Directory);
                 return r->done(i);
               }
               r->request(
                   [=](util::Output input) {
                     return getItemDataRequest(id, *input);
                   },
                   [=](EitherError<Response> e) {
                     if (e.left()) return r->done(e.left());
                     auto id_data = from_string(id);
                     try {
                       r->done(getItemDataResponse(e.right()->output(),
                                                   id_data.type));
                     } catch (const Json::Exception&) {
                       r->done(Error{IHttpRequest::Failure,
                                     e.right()->output().str()});
                     }
                   });
             })
      ->run();
}

ICloudProvider::DownloadFileRequest::Pointer YouTube::downloadFileAsync(
    IItem::Pointer i, IDownloadFileCallback::Pointer cb, Range range) {
  auto file_id = from_string(i->id());
  if (!(file_id.type & YouTubeItem::HighQuality) ||
      (file_id.type & YouTubeItem::DontMerge))
    return std::make_shared<DownloadFileFromUrlRequest>(shared_from_this(), i,
                                                        cb, range)

        ->run();
  auto resolver = [=](Request<EitherError<void>>::Pointer r) {
    get_stream<EitherError<void>>(r, i, [=](EitherError<std::string> e) {
      if (e.left()) return r->done(e.left());
      auto manifest = generateDashManifest(*i, *e.right());
      auto drange = range;
      if (drange.size_ == Range::Full)
        drange.size_ = manifest.size() - range.start_;
      cb->receivedData(manifest.data() + drange.start_, drange.size_);
      cb->progress(drange.size_, drange.size_);
      r->done(nullptr);
    });
  };
  return std::make_shared<Request<EitherError<void>>>(
             shared_from_this(), [=](EitherError<void> e) { cb->done(e); },
             resolver)
      ->run();
}

ICloudProvider::GetItemUrlRequest::IRequest::Pointer YouTube::getItemUrlAsync(
    IItem::Pointer item, GetItemUrlCallback callback) {
  auto type = from_string(item->id()).type;
  return std::make_shared<Request<EitherError<std::string>>>(
             shared_from_this(), callback,
             [=](Request<EitherError<std::string>>::Pointer r) {
               get_stream<EitherError<std::string>>(
                   r, item, [=](EitherError<std::string> e) {
                     if (e.right() && (type & YouTubeItem::HighQuality) &&
                         !(type & YouTubeItem::DontMerge)) {
                       r->done(defaultFileDaemonUrl(
                           *item,
                           generateDashManifest(*item, *e.right()).size()));
                     } else
                       r->done(e);
                   });
             })
      ->run();
}

IHttpRequest::Pointer YouTube::getItemDataRequest(const std::string& full_id,
                                                  std::ostream&) const {
  auto id_data = from_string(full_id);
  if (!(id_data.type & YouTubeItem::Playlist)) {
    auto request =
        http()->create(endpoint() + "/youtube/v3/playlistItems", "GET");
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

IHttpRequest::Pointer YouTube::getGeneralDataRequest(std::ostream&) const {
  return http()->create("https://www.googleapis.com/plus/v1/people/me");
}

IHttpRequest::Pointer YouTube::deleteItemRequest(const IItem& item,
                                                 std::ostream&) const {
  if (item.id() == rootDirectory()->id() || item.id() == AUDIO_DIRECTORY_ID ||
      item.id() == HIGH_QUALITY_DIRECTORY_ID)
    return nullptr;
  auto id_data = from_string(item.id());
  if (id_data.type & YouTubeItem::RelatedPlaylist) return nullptr;
  auto request = http()->create(
      endpoint() + "/youtube/v3/" +
          ((id_data.type & YouTubeItem::Playlist) ? "playlists"
                                                  : "playlistItems"),
      "DELETE");
  request->setParameter("id", id_data.id);
  return request;
}

IHttpRequest::Pointer YouTube::renameItemRequest(const IItem& item,
                                                 const std::string& name,
                                                 std::ostream& stream) const {
  if (item.id() == rootDirectory()->id() || item.id() == AUDIO_DIRECTORY_ID ||
      item.id() == HIGH_QUALITY_DIRECTORY_ID)
    return nullptr;
  auto id_data = from_string(item.id());
  if (!(id_data.type & YouTubeItem::Playlist)) return nullptr;
  auto request = http()->create(endpoint() + "/youtube/v3/playlists", "PUT");
  request->setParameter("part", "snippet");
  request->setHeaderParameter("Content-Type", "application/json");
  Json::Value argument;
  argument["id"] = id_data.id;
  argument["snippet"]["title"] = name;
  stream << argument;
  return request;
}

IHttpRequest::Pointer YouTube::createDirectoryRequest(
    const IItem& item, const std::string& name, std::ostream& stream) const {
  if (item.id() != rootDirectory()->id() && item.id() != AUDIO_DIRECTORY_ID &&
      item.id() != HIGH_QUALITY_DIRECTORY_ID)
    return nullptr;
  auto request = http()->create(endpoint() + "/youtube/v3/playlists", "POST");
  request->setParameter("part", "snippet");
  request->setHeaderParameter("Content-Type", "application/json");
  Json::Value argument;
  argument["snippet"]["title"] = name;
  stream << argument;
  return request;
}

IHttpRequest::Pointer YouTube::listDirectoryRequest(
    const IItem& item, const std::string& page_token, std::ostream&) const {
  if (item.id() == rootDirectory()->id() || item.id() == AUDIO_DIRECTORY_ID ||
      item.id() == HIGH_QUALITY_DIRECTORY_ID) {
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
                                            int type) const {
  auto response = util::json::from_stream(stream);
  return toItem(response["items"][0], response["kind"].asString(), type);
}

IItem::List YouTube::listDirectoryResponse(const IItem& directory,
                                           std::istream& stream,
                                           std::string& next_page_token) const {
  std::unique_ptr<Json::CharReader> reader(
      Json::CharReaderBuilder().newCharReader());
  std::stringstream sstream;
  sstream << stream.rdbuf();
  std::string str = sstream.str();
  Json::Value response;
  reader->parse(str.data(), str.data() + str.size(), &response, nullptr);
  IItem::List result;
  auto type = from_string(directory.id()).type;
  if (response["kind"].asString() == "youtube#channelListResponse") {
    Json::Value related_playlists =
        response["items"][0]["contentDetails"]["relatedPlaylists"];
    for (const std::string& name : related_playlists.getMemberNames()) {
      if (name == "watchLater" || name == "watchHistory" || name == "favorites")
        continue;
      auto item = util::make_unique<Item>(
          related_playlist_name(name),
          to_string(
              {YouTubeItem::Playlist | YouTubeItem::RelatedPlaylist | type,
               related_playlists[name].asString(), ""}),
          IItem::UnknownSize, IItem::UnknownTimeStamp,
          IItem::FileType::Directory);
      item->set_thumbnail_url(
          response["items"][0]["snippet"]["thumbnails"]["default"]["url"]
              .asString());
      result.push_back(std::move(item));
    }
    next_page_token = "real_playlist";
  } else {
    for (const Json::Value& v : response["items"]) {
      auto item =
          toItem(v, response["kind"].asString(),
                 type & (YouTubeItem::Audio | YouTubeItem::HighQuality));
      if (!item->thumbnail_url().empty()) result.push_back(item);
    }
  }
  if (response.isMember("nextPageToken"))
    next_page_token = response["nextPageToken"].asString();
  else if (directory.id() == rootDirectory()->id() && next_page_token.empty()) {
    result.push_back(util::make_unique<Item>(
        AUDIO_DIRECTORY, AUDIO_DIRECTORY_ID, IItem::UnknownSize,
        IItem::UnknownTimeStamp, IItem::FileType::Directory));
    result.push_back(util::make_unique<Item>(
        HIGH_QUALITY_DIRECTORY, HIGH_QUALITY_DIRECTORY_ID, IItem::UnknownSize,
        IItem::UnknownTimeStamp, IItem::FileType::Directory));
  }
  return result;
}

GeneralData YouTube::getGeneralDataResponse(std::istream& response) const {
  auto json = util::json::from_stream(response);
  GeneralData data;
  data.space_total_ = 0;
  data.space_used_ = 0;
  for (auto&& j : json["emails"])
    if (j["type"].asString() == "account")
      data.username_ = j["value"].asString();
  return data;
}

IItem::Pointer YouTube::renameItemResponse(const IItem& old_item,
                                           const std::string&,
                                           std::istream& response) const {
  auto json = util::json::from_stream(response);
  if (json.isMember("error"))
    throw std::logic_error(json["error"]["errors"][0]["message"].asString());
  auto id = from_string(old_item.id());
  return toItem(json, "youtube#playlistListResponse", id.type);
}

IItem::Pointer YouTube::createDirectoryResponse(const IItem& parent,
                                                const std::string&,
                                                std::istream& response) const {
  auto id = from_string(parent.id());
  return toItem(util::json::from_stream(response),
                "youtube#playlistListResponse", id.type);
}

std::string YouTube::generateDashManifest(const IItem& i,
                                          const std::string& url) const {
  auto data = util::parse_form(util::Url(url).query());
  auto id = from_string(i.id());
  id.type =
      YouTubeItem::DontMerge | YouTubeItem::HighQuality | YouTubeItem::Audio;
  auto item_audio = util::make_unique<Item>(i.filename(), to_string(id),
                                            std::stoull(data["audio_size"]),
                                            i.timestamp(), i.type());
  item_audio->set_url(data["audio"]);
  id.type = YouTubeItem::DontMerge | YouTubeItem::HighQuality;
  auto item_video = util::make_unique<Item>(i.filename(), to_string(id),
                                            std::stoull(data["video_size"]),
                                            i.timestamp(), i.type());
  item_video->set_url(data["video"]);
  auto audio_url = defaultFileDaemonUrl(*item_audio, item_audio->size());
  auto video_url = defaultFileDaemonUrl(*item_video, item_video->size());
  return generate_dash_manifest(
      url, util::parse_form(util::Url(data["video"]).query())["dur"], audio_url,
      video_url);
}

Item::Pointer YouTube::toItem(const Json::Value& v, std::string kind,
                              int type) const {
  if (kind == "youtube#playlistListResponse") {
    auto item = util::make_unique<Item>(
        v["snippet"]["title"].asString(),
        to_string({type, v["id"].asString(), ""}), IItem::UnknownSize,
        util::parse_time(v["snippet"]["publishedAt"].asString()),
        IItem::FileType::Directory);
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
      throw std::logic_error(util::Error::INVALID_KIND);

    auto item = util::make_unique<Item>(
        v["snippet"]["title"].asString() +
            ((type & YouTubeItem::HighQuality) ? ".mpd" : ".mp4"),
        to_string({type, v["id"].asString(), video_id}), IItem::UnknownSize,
        util::parse_time(v["snippet"]["publishedAt"].asString()),
        (type & YouTubeItem::Audio) ? IItem::FileType::Audio
                                    : IItem::FileType::Video);
    item->set_thumbnail_url(
        v["snippet"]["thumbnails"]["default"]["url"].asString());
    return std::move(item);
  }
}

std::string YouTube::Auth::authorizeLibraryUrl() const {
  return "https://accounts.google.com/o/oauth2/auth?client_id=" + client_id() +
         "&redirect_uri=" + redirect_uri() +
         "&scope=https://www.googleapis.com/auth/youtube+https://"
         "www.googleapis.com/auth/plus.profile.emails.read"
         "&response_type=code&access_type=offline&prompt=consent"
         "&state=" +
         state();
}

}  // namespace cloudstorage
