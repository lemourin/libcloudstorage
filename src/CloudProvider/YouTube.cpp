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

const std::string AUDIO_DIRECTORY = "audio";
const std::string AUDIO_DIRECTORY_ID =
    "eyJhdWRpbyI6dHJ1ZSwicGxheWxpc3QiOnRydWUsImlkIjoiYXVkaW8ifQo=";

const std::string HIGH_QUALITY_DIRECTORY = "high quality";
const std::string HIGH_QUALITY_DIRECTORY_ID =
    "eyJhdWRpbyI6ZmFsc2UsInBsYXlsaXN0Ijp0cnVlLCJoaWdoX3F1YWxpdHkiOnRydWUsImlkIj"
    "oiaGlnaCBxdWFsaXR5In0K";

using namespace std::placeholders;

namespace cloudstorage {

namespace {

struct YouTubeItem {
  bool audio;
  bool playlist;
  bool high_quality;
  std::string id;
};

struct VideoInfo {
  std::string scrambled_signature;
  std::string signature;
  std::string type;
  std::string url;
  int bitrate;
};

YouTubeItem from_string(const std::string& id) {
  try {
    auto json =
        util::json::from_stream(std::stringstream(util::from_base64(id)));
    return {json["audio"].asBool(), json["playlist"].asBool(),
            json["high_quality"].asBool(), json["id"].asString()};
  } catch (const Json::Exception&) {
    return {};
  }
}

std::string to_string(YouTubeItem item) {
  Json::Value json;
  json["audio"] = item.audio;
  json["playlist"] = item.playlist;
  json["high_quality"] = item.high_quality;
  json["id"] = item.id;
  return util::to_base64(util::json::to_string(json));
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
    else if (key == "type")
      result.type = util::Url::unescape(value);
    else if (key == "url")
      result.url = util::Url::unescape(value);
    else if (key == "bitrate")
      result.bitrate = std::stoi(value);
  }
  return result;
}

std::string combine(const std::string& video, const std::string& audio) {
  return "cloudstorage://youtube/?video=" + util::Url::escape(video) +
         "&audio=" + util::Url::escape(audio);
}

EitherError<std::string> descramble(const std::string& scrambled,
                                    std::stringstream& stream) {
  auto find_descrambler = [](std::stringstream& stream) {
    auto player = stream.str();
    const std::string descrambler_search = "\"signature\":\"sig\";c=";
    auto it = player.find(descrambler_search);
    if (it == std::string::npos)
      throw std::logic_error("can't find descrambler name");
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
      throw std::logic_error("can't find helper functions");
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
      throw std::logic_error("can't find descrambler definition");
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
        throw std::logic_error("invalid transformation function");
      auto value = std::stoull(arg);
      if (func->second.find("splice") != std::string::npos)
        code.erase(0, value);
      else if (func->second.find("reverse") != std::string::npos)
        std::reverse(code.begin(), code.end());
      else if (func->second.find("a[0]=a[b%a.length];") != std::string::npos)
        std::swap(code[0], code[value % code.length()]);
      else
        throw std::logic_error("unknown transformation");
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
         GetItemUrl;
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
                                                   id_data.audio,
                                                   id_data.high_quality));
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
  return std::make_shared<DownloadFileFromUrlRequest>(shared_from_this(), i, cb,
                                                      range)
      ->run();
}

ICloudProvider::GetItemUrlRequest::IRequest::Pointer YouTube::getItemUrlAsync(
    IItem::Pointer item, GetItemUrlCallback callback) {
  auto get_config = [](std::stringstream& stream) {
    std::string page = stream.str();
    std::string player_str = "ytplayer.config = ";
    auto it = page.find(player_str);
    if (it == std::string::npos)
      throw std::logic_error("ytplayer.config not found");
    stream.seekg(it + player_str.length());
    return util::json::from_stream(stream);
  };
  auto get_url = [=](Request<EitherError<std::string>>::Pointer r,
                     Json::Value json) {
    auto data = from_string(item->id());
    std::stringstream stream(
        item->type() == IItem::FileType::Audio || data.high_quality
            ? json["args"]["adaptive_fmts"].asString()
            : json["args"]["url_encoded_fmt_stream_map"].asString());
    std::string url;
    VideoInfo best_audio, best_video;
    best_audio.bitrate = best_video.bitrate = -1;
    while (std::getline(stream, url, ',')) {
      auto d = video_info(url);
      if (d.type.find("video") != std::string::npos &&
          d.bitrate > best_video.bitrate)
        best_video = d;
      else if (d.type.find("audio") != std::string::npos &&
               d.bitrate > best_audio.bitrate)
        best_audio = d;
    }
    if (!data.audio && !data.high_quality) best_audio = best_video;
    if (best_video.url.empty()) throw std::logic_error("video url not found");
    if (best_audio.url.empty()) throw std::logic_error("audio url not found");
    if (best_video.scrambled_signature.empty() &&
        best_audio.scrambled_signature.empty()) {
      if (!data.high_quality)
        return r->done(data.audio ? best_audio.url : best_video.url);
      else
        return r->done(combine(best_video.url, best_audio.url));
    }
    r->send(
        [=](util::Output) {
          return http()->create("http://youtube.com" +
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
          if (!data.high_quality)
            return r->done(data.audio ? audio_url : video_url);
          else
            return r->done(combine(video_url, audio_url));
        });
  };
  return std::make_shared<Request<EitherError<std::string>>>(
             shared_from_this(), callback,
             [=](Request<EitherError<std::string>>::Pointer r) {
               r->send(
                   [=](util::Output) {
                     return http()->create("http://youtube.com/watch?v=" +
                                           from_string(item->id()).id);
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
             })
      ->run();
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

IHttpRequest::Pointer YouTube::getGeneralDataRequest(std::ostream&) const {
  return http()->create("https://www.googleapis.com/plus/v1/people/me");
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

IItem::Pointer YouTube::getItemDataResponse(std::istream& stream, bool audio,
                                            bool high_quality) const {
  auto response = util::json::from_stream(stream);
  return toItem(response["items"][0], response["kind"].asString(), audio,
                high_quality);
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
  bool audio = from_string(directory.id()).audio;
  bool high_quality = from_string(directory.id()).high_quality;
  if (response["kind"].asString() == "youtube#channelListResponse") {
    Json::Value related_playlists =
        response["items"][0]["contentDetails"]["relatedPlaylists"];
    for (const std::string& name : related_playlists.getMemberNames()) {
      auto item = util::make_unique<Item>(
          name, to_string({audio, true, high_quality,
                           related_playlists[name].asString()}),
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
      auto item = toItem(v, response["kind"].asString(), audio, high_quality);
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

Item::Pointer YouTube::toItem(const Json::Value& v, std::string kind,
                              bool audio, bool high_quality) const {
  if (kind == "youtube#playlistListResponse") {
    auto item = util::make_unique<Item>(
        v["snippet"]["title"].asString(),
        to_string({audio, true, high_quality, v["id"].asString()}),
        IItem::UnknownSize, IItem::UnknownTimeStamp,
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
      throw std::logic_error("invalid kind");

    auto item = util::make_unique<Item>(
        v["snippet"]["title"].asString() + (audio ? ".webm" : ".mp4"),
        to_string({audio, false, high_quality, video_id}), IItem::UnknownSize,
        IItem::UnknownTimeStamp,
        audio ? IItem::FileType::Audio : IItem::FileType::Video);
    item->set_thumbnail_url(
        v["snippet"]["thumbnails"]["default"]["url"].asString());
    return std::move(item);
  }
}

std::string YouTube::Auth::authorizeLibraryUrl() const {
  return "https://accounts.google.com/o/oauth2/auth?client_id=" + client_id() +
         "&redirect_uri=" + redirect_uri() +
         "&scope=https://www.googleapis.com/auth/youtube.readonly+https://"
         "www.googleapis.com/auth/plus.profile.emails.read"
         "&response_type=code&access_type=offline&prompt=consent"
         "&state=" +
         state();
}

}  // namespace cloudstorage
