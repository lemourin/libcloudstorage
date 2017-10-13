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

EitherError<std::string> descramble(const std::string& scrambled,
                                    std::stringstream& stream) {
  auto find_descrambler = [](std::stringstream& stream) {
    auto player = stream.str();
    const std::string descrambler_search = ".set(\"signature\",";
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
      auto value = std::stol(arg);
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
  } catch (const std::exception& e) {
    return Error{IHttpRequest::Failure, e.what()};
  }
}

}  // namespace

YouTube::YouTube() : CloudProvider(util::make_unique<Auth>()) {}

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
               r->request(
                   [=](util::Output input) {
                     return getItemDataRequest(id, *input);
                   },
                   [=](EitherError<Response> e) {
                     if (e.left()) return r->done(e.left());
                     auto id_data = from_string(id);
                     try {
                       r->done(getItemDataResponse(e.right()->output(),
                                                   id_data.audio));
                     } catch (std::exception) {
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
    Json::Value json;
    stream >> json;
    return json;
  };
  auto get_url = [=](Request<EitherError<std::string>>::Pointer r,
                     Json::Value json) {
    std::stringstream stream(
        item->type() == IItem::FileType::Audio
            ? json["args"]["adaptive_fmts"].asString()
            : json["args"]["url_encoded_fmt_stream_map"].asString());
    std::string token, signature, scrambled_signature, url, video_url;
    while (std::getline(stream, url, ',')) {
      std::stringstream ss(url);
      std::string type;
      while (std::getline(ss, token, '&')) {
        std::stringstream stream(token);
        std::string key, value;
        std::getline(stream, key, '=');
        std::getline(stream, value, '=');
        if (key == "s")
          scrambled_signature = value;
        else if (key == "sig")
          signature = "&signature=" + value;
        else if (key == "type")
          type = util::Url::unescape(value);
        else if (key == "url")
          video_url = util::Url::unescape(value);
      }
      auto type_str =
          item->type() == IItem::FileType::Video ? "video" : "audio/webm";
      if (type.find(type_str) != std::string::npos) break;
    }
    if (video_url.empty()) throw std::logic_error("url not found");
    if (scrambled_signature.empty()) return r->done(video_url + signature);
    r->send(
        [=](util::Output) {
          return http()->create("http://youtube.com" +
                                json["assets"]["js"].asString());
        },
        [=](EitherError<Response> e) {
          if (e.left()) return r->done(e.left());
          auto signature = descramble(scrambled_signature, e.right()->output());
          if (signature.left())
            r->done(signature.left());
          else
            r->done(video_url + "&signature=" + *signature.right());
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
    return std::move(item);
  }
}

std::string YouTube::Auth::authorizeLibraryUrl() const {
  return "https://accounts.google.com/o/oauth2/auth?client_id=" + client_id() +
         "&redirect_uri=" + redirect_uri() +
         "&scope=https://www.googleapis.com/auth/youtube.readonly"
         "&response_type=code&access_type=offline&prompt=consent"
         "&state=" +
         state();
}

}  // namespace cloudstorage
