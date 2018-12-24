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

const int CACHE_SIZE = 1024;

using namespace std::placeholders;

namespace cloudstorage {

namespace {

struct YouTubeDescrambleData {
  std::string function;
  std::string helper;
};

struct YouTubeItem {
  enum Type {
    Audio = 1 << 0,
    Playlist = 1 << 1,
    HighQuality = 1 << 2,
    RelatedPlaylist = 1 << 3,
    Stream = 1 << 4
  };
  int type = 0;
  std::string id;
  std::string video_id;
  std::string name;
  IItem::TimeStamp timestamp;

  YouTubeItem() {}

  YouTubeItem(int type, const std::string& id, const std::string& video_id,
              const std::string& name,
              const IItem::TimeStamp& timestamp = IItem::UnknownTimeStamp)
      : type(type),
        id(id),
        video_id(video_id),
        name(name),
        timestamp(timestamp) {}
};

YouTubeItem from_string(const std::string& id) {
  try {
    auto json =
        util::json::from_stream(std::stringstream(util::from_base64(id)));
    YouTubeItem result = {json["type"].asInt(), json["id"].asString(),
                          json["video_id"].asString(), ""};
    result.name = json["name"].asString();
    result.timestamp =
        std::chrono::system_clock::from_time_t(json["timestamp"].asInt64());
    return result;
  } catch (const Json::Exception&) {
    return {};
  }
}

std::string to_string(const YouTubeItem& item) {
  Json::Value json;
  json["type"] = item.type;
  json["id"] = item.id;
  json["video_id"] = item.video_id;
  json["name"] = item.name;
  json["timestamp"] = static_cast<Json::Int64>(
      std::chrono::system_clock::to_time_t(item.timestamp));
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

bool find(std::stringstream& stream, const std::string& pattern) {
  std::deque<char> buffer;
  stream.seekg(0);
  while (true) {
    char c;
    if (!stream.read(&c, 1)) {
      break;
    }
    buffer.push_back(c);
    if (buffer.size() > pattern.size()) {
      buffer.pop_front();
    }
    if (std::equal(buffer.begin(), buffer.end(), pattern.begin(),
                   pattern.end())) {
      return true;
    }
  }
  return false;
}

YouTubeDescrambleData descramble_data(std::stringstream& stream) {
  auto find_descrambler = [](std::stringstream& stream) {
    const std::string descrambler_search =
        "(k.sp,(0,window.encodeURIComponent)(";
    if (!find(stream, descrambler_search))
      throw std::logic_error(util::Error::COULD_NOT_FIND_DESCRAMBLER_NAME);
    std::string descrambler;
    std::getline(stream, descrambler, '(');
    return descrambler;
  };
  auto find_descrambler_code = [](const std::string& name,
                                  std::stringstream& stream) {
    const std::string function_search = name + "=function(a){";
    if (!find(stream, function_search))
      throw std::logic_error(
          util::Error::COULD_NOT_FIND_DESCRABMLER_DEFINITION);
    std::string code;
    std::getline(stream, code, ';');
    std::getline(stream, code, '}');
    return code.substr(0, code.find_last_of(';') + 1);
  };
  auto find_helper = [](const std::string& code, std::stringstream& stream) {
    auto helper = code.substr(0, code.find_first_of('.'));
    const std::string helper_search = "var " + helper + "={";
    if (!find(stream, helper_search))
      throw std::logic_error(util::Error::COULD_NOT_FIND_HELPER_FUNCTIONS);
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
  YouTubeDescrambleData data;
  data.function = find_descrambler_code(find_descrambler(stream), stream);
  data.helper = find_helper(data.function, stream);
  return data;
}

EitherError<std::string> descramble(const std::string& scrambled,
                                    const YouTubeDescrambleData& data) {
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
  return transform(scrambled, data.function, transformations(data.helper));
}

template <class Result>
void get_stream(
    typename Request<Result>::Pointer r, const std::string& video_id,
    std::function<void(EitherError<std::vector<VideoInfo>>)> complete) {
  auto get_config = [](std::stringstream& stream) {
    const std::string player_str = "ytplayer.config = ";
    if (!find(stream, player_str))
      throw std::logic_error(util::Error::YOUTUBE_CONFIG_NOT_FOUND);
    return util::json::from_stream(stream);
  };
  auto get_url = [=](typename Request<Result>::Pointer r, Json::Value json) {
    std::vector<VideoInfo> result;
    bool scrambled_signature = false;
    for (const auto& data :
         {std::make_pair(json["args"]["adaptive_fmts"].asString(), true),
          std::make_pair(json["args"]["url_encoded_fmt_stream_map"].asString(),
                         false)}) {
      std::stringstream stream(data.first);
      std::string url;
      while (std::getline(stream, url, ',')) {
        auto d = video_info(url);
        d.adaptive_ = data.second;
        result.push_back(d);
        if (!d.scrambled_signature.empty()) scrambled_signature = true;
      }
    }
    if (!scrambled_signature) return complete(result);
    r->send(
        [=](util::Output) {
          return r->provider()->http()->create("http://youtube.com" +
                                               json["assets"]["js"].asString());
        },
        [=](EitherError<Response> e) {
          if (e.left()) return complete(e.left());
          try {
            std::vector<VideoInfo> decoded;
            auto data = descramble_data(e.right()->output());
            for (auto v : result) {
              if (!v.scrambled_signature.empty()) {
                auto signature = descramble(v.scrambled_signature, data);
                if (signature.left()) {
                  return complete(signature.left());
                }
                v.url += "&signature=" + *signature.right();
                decoded.emplace_back(v);
              } else {
                decoded.emplace_back(v);
              }
            }
            complete(decoded);
          } catch (const std::logic_error& e) {
            complete(Error{IHttpRequest::Failure, e.what()});
          }
        });
  };
  r->send(
      [=](util::Output) {
        return r->provider()->http()->create("http://youtube.com/watch?v=" +
                                             video_id);
      },
      [=](EitherError<Response> e) {
        if (e.left()) return complete(e.left());
        try {
          auto json = get_config(e.right()->output());
          get_url(r, json);
        } catch (const std::exception& e) {
          complete(Error{IHttpRequest::Failure, e.what()});
        }
      });
}

std::string generate_dash_manifest(const std::string& duration,
                                   const VideoInfo& video_stream,
                                   const VideoInfo& audio_stream) {
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
         << audio_stream.codec <<
      R"(" contentType="audio" subsegmentAlignment="true" subsegmentStartsWithSAP="1">
        <Representation id="1" bandwidth=")"
         << audio_stream.bitrate << R"(">
          <BaseURL>)"
         << xml_escape(audio_stream.url) << R"(</BaseURL>
          <SegmentBase indexRange=")"
         << audio_stream.index_range << R"(">
            <Initialization range=")"
         << audio_stream.init_range << R"("/>
          </SegmentBase>
        </Representation>
      </AdaptationSet>
      <AdaptationSet mimeType="video/mp4" codecs=")"
         << video_stream.codec <<
      R"(" contentType="video" subsegmentAlignment="true" subsegmentStartsWithSAP="1">
        <Representation id="2" bandwidth=")"
         << video_stream.bitrate << R"(">
          <BaseURL>)"
         << xml_escape(video_stream.url) << R"(</BaseURL>
          <SegmentBase indexRange=")"
         << video_stream.index_range << R"(">
            <Initialization range=")"
         << video_stream.init_range << R"("/>
          </SegmentBase>
        </Representation>
      </AdaptationSet>
    </Period>
  </MPD>)";
  return stream.str();
}

VideoInfo get_stream(const std::vector<VideoInfo>& d, int type) {
  VideoInfo result = {};
  result.bitrate = -1;
  for (const auto& d : d) {
    if (d.bitrate > result.bitrate) {
      if (type & YouTubeItem::Audio) {
        if (d.type.find("audio/mp4") != std::string::npos && d.adaptive_)
          result = d;
      } else {
        if (d.type.find("video/mp4") != std::string::npos &&
            d.adaptive_ == bool(type & YouTubeItem::Stream))
          result = d;
      }
    }
  }
  return result;
}

}  // namespace

YouTube::YouTube()
    : CloudProvider(util::make_unique<Auth>()), manifest_data_(CACHE_SIZE) {}

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
               auto data = from_string(id);
               if (data.type & YouTubeItem::Playlist) {
                 IItem::Pointer result = std::make_shared<Item>(
                     data.name, id, IItem::UnknownSize, data.timestamp,
                     IItem::FileType::Directory);
                 return r->done(result);
               }
               auto i = std::make_shared<Item>(
                   data.name, id, IItem::UnknownSize, data.timestamp,
                   (data.type & YouTubeItem::Audio) ? IItem::FileType::Audio
                                                    : IItem::FileType::Video);
               bool is_manifest = (data.type & YouTubeItem::HighQuality) &&
                                  !(data.type & YouTubeItem::Stream);
               if (!is_manifest) {
                 auto entry = manifest_data_.get(data.video_id);
                 if (entry) {
                   auto track = get_stream(*entry, data.type);
                   i->set_url(track.url);
                   if (track.size > 0) i->set_size(track.size);
                 }
               }
               return r->done(std::static_pointer_cast<IItem>(i));
             })
      ->run();
}

ICloudProvider::DownloadFileRequest::Pointer YouTube::downloadFileAsync(
    IItem::Pointer i, IDownloadFileCallback::Pointer cb, Range range) {
  auto file_id = from_string(i->id());
  if (!(file_id.type & YouTubeItem::HighQuality) ||
      (file_id.type & YouTubeItem::Stream))
    return std::make_shared<DownloadFileFromUrlRequest>(shared_from_this(), i,
                                                        cb, range)

        ->run();
  auto put_data = [=](Request<EitherError<void>>::Pointer r,
                      const std::vector<VideoInfo>& video_info) {
    auto manifest = generateDashManifest(*i, video_info);
    auto drange = range;
    if (drange.size_ == Range::Full)
      drange.size_ = manifest.size() - range.start_;
    cb->receivedData(manifest.data() + drange.start_, drange.size_);
    cb->progress(drange.size_, drange.size_);
    r->done(nullptr);
  };
  auto resolver = [=](Request<EitherError<void>>::Pointer r) {
    if (auto entry = manifest_data_.get(file_id.video_id)) {
      put_data(r, *entry);
    } else {
      get_stream<EitherError<void>>(
          r, file_id.video_id, [=](EitherError<std::vector<VideoInfo>> e) {
            if (e.left())
              r->done(e.left());
            else {
              manifest_data_.put(file_id.video_id, e.right());
              put_data(r, *e.right());
            }
          });
    }
  };
  return std::make_shared<Request<EitherError<void>>>(
             shared_from_this(), [=](EitherError<void> e) { cb->done(e); },
             resolver)
      ->run();
}

ICloudProvider::GetItemUrlRequest::IRequest::Pointer YouTube::getItemUrlAsync(
    IItem::Pointer item, GetItemUrlCallback callback) {
  auto file_id = from_string(item->id());
  auto type = file_id.type;
  bool is_manifest =
      (type & YouTubeItem::HighQuality) && !(type & YouTubeItem::Stream);
  return std::make_shared<Request<EitherError<std::string>>>(
             shared_from_this(), callback,
             [=](Request<EitherError<std::string>>::Pointer r) {
               if (is_manifest)
                 if (auto entry = manifest_data_.get(file_id.video_id))
                   return r->done(defaultFileDaemonUrl(
                       *item, generateDashManifest(*item, *entry).size()));
               get_stream<EitherError<std::string>>(
                   r, file_id.video_id,
                   [=](EitherError<std::vector<VideoInfo>> e) {
                     if (e.left()) return r->done(e.left());
                     manifest_data_.put(file_id.video_id, e.right());
                     if (is_manifest) {
                       r->done(defaultFileDaemonUrl(
                           *item,
                           generateDashManifest(*item, *e.right()).size()));
                     } else {
                       r->done(get_stream(*e.right(), type).url);
                     }
                   });
             })
      ->run();
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
  if ((id_data.type & YouTubeItem::RelatedPlaylist) &&
      (id_data.type & YouTubeItem::Playlist))
    return nullptr;
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
               related_playlists[name].asString(), "", ""}),
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
      auto item = toItem(v, response["kind"].asString(),
                         type & (YouTubeItem::Audio | YouTubeItem::HighQuality |
                                 YouTubeItem::RelatedPlaylist));
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

std::string YouTube::generateDashManifest(
    const IItem& i, const std::vector<VideoInfo>& video_info) const {
  VideoInfo best_video, best_audio;
  best_audio.bitrate = best_video.bitrate = -1;
  for (const auto& v : video_info)
    if (v.adaptive_) {
      if (v.type.find("audio/mp4") != std::string::npos &&
          v.bitrate > best_audio.bitrate)
        best_audio = v;
      else if (v.type.find("video/mp4") != std::string::npos &&
               v.bitrate > best_video.bitrate)
        best_video = v;
    }
  auto id = from_string(i.id());
  id.type = YouTubeItem::Stream | YouTubeItem::HighQuality |
            YouTubeItem::Audio | (id.type & YouTubeItem::RelatedPlaylist);
  auto item_audio = util::make_unique<Item>(
      i.filename(), to_string(id), best_audio.size, i.timestamp(), i.type());
  item_audio->set_url(best_audio.url);
  id.type = YouTubeItem::Stream | YouTubeItem::HighQuality |
            (id.type & YouTubeItem::RelatedPlaylist);
  auto item_video = util::make_unique<Item>(
      i.filename(), to_string(id), best_video.size, i.timestamp(), i.type());
  item_video->set_url(best_video.url);
  auto duration = util::parse_form(util::Url(best_video.url).query())["dur"];
  best_video.url = defaultFileDaemonUrl(*item_video, item_video->size());
  best_audio.url = defaultFileDaemonUrl(*item_audio, item_audio->size());
  auto result = generate_dash_manifest(duration, best_video, best_audio);
  return result;
}

Item::Pointer YouTube::toItem(const Json::Value& v, std::string kind,
                              int type) const {
  if (kind == "youtube#playlistListResponse") {
    auto item = util::make_unique<Item>(
        v["snippet"]["title"].asString(),
        to_string({type | YouTubeItem::Playlist, v["id"].asString(), "", ""}),
        IItem::UnknownSize,
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

    auto timestamp = util::parse_time(v["snippet"]["publishedAt"].asString());
    auto name = v["snippet"]["title"].asString() +
                ((type & YouTubeItem::HighQuality) ? ".mpd" : ".mp4");
    auto item = util::make_unique<Item>(
        name, to_string({type, v["id"].asString(), video_id, name, timestamp}),
        IItem::UnknownSize, timestamp,
        (type & YouTubeItem::Audio) ? IItem::FileType::Audio
                                    : IItem::FileType::Video);
    item->set_thumbnail_url(
        v["snippet"]["thumbnails"]["default"]["url"].asString());
    bool is_manifest =
        (type & YouTubeItem::HighQuality) && !(type & YouTubeItem::Stream);
    if (!is_manifest) {
      auto entry = manifest_data_.get(video_id);
      if (entry) {
        VideoInfo info = get_stream(*entry, type);
        item->set_url(info.url);
        if (info.size > 0) item->set_size(info.size);
      }
    }
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
