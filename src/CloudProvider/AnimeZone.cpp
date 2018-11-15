/*****************************************************************************
 * AnimeZone.cpp
 *
 *****************************************************************************
 * Copyright (C) 2018 VideoLAN
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
#include "AnimeZone.h"
#include <json/json.h>
#include <regex>
#include <unordered_set>
#include "Request/DownloadFileRequest.h"
#include "Request/Request.h"
#include "Utility/Item.h"
#include "Utility/Utility.h"

namespace re = std;

namespace cloudstorage {

const auto USER_AGENT =
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
    "AppleWebKit/537.36 (KHTML, like Gecko) "
    "Chrome/64.0.3282.167 Safari/537.36";

namespace {

std::string extract_session(const IHttpRequest::HeaderParameters &headers) {
  auto cookie_range = headers.equal_range("set-cookie");
  for (auto it = cookie_range.first; it != cookie_range.second; ++it) {
    re::regex sess_rx("_SESS=([^;]*);");
    re::smatch match;
    if (re::regex_search(it->second, match, sess_rx)) {
      return match[1].str();
    }
  }
  return "";
}

struct PlayerDetails {
  std::string name_;
  std::string code_;
  std::string language_;
};

std::vector<PlayerDetails> episode_to_players(const std::string &page) {
  std::vector<PlayerDetails> result;
  auto start = page.find("Wszystkie odcinki");
  if (start == std::string::npos) {
    throw std::logic_error(util::Error::PLAYERS_NOT_FOUND);
  }
  re::regex button_rx(
      "<td>([^<]*)</td>(?:[^\\n]*\\n){2}.*?sprites (.*?) "
      "lang(?:[^\\n]*\\n){2}.*?data-[^\"]*\"([^\"]*)\"");
  for (auto it = re::sregex_iterator(page.begin() + static_cast<int64_t>(start),
                                     page.end(), button_rx);
       it != re::sregex_iterator(); ++it) {
    result.push_back(
        PlayerDetails{(*it)[1].str(), (*it)[3].str(), (*it)[2].str()});
  }
  return result;
}

}  // namespace

namespace openload {

std::string cipher(const std::string &page) {
  std::string ciphered_begin =
      "<div class=\"\" style=\"display:none;\">\n<p style=\"\" id=\"";
  auto begin =
      page.find('>', page.find(ciphered_begin) + ciphered_begin.length()) + 1;
  auto end = page.find('<', begin) - 1;
  return page.substr(begin, end - begin + 1);
}

std::vector<std::string> values(const std::string &page) {
  std::string b1search = "var  _1x4bfb36=parseInt('";
  auto r1begin = page.find(b1search) + b1search.length();
  auto r1end = page.find("'", r1begin);
  auto r2begin = page.find('-', r1end) + 1;
  auto r2end = page.find(';', r2begin);

  std::string b2search = "_0x30725e,(parseInt('";
  auto r3begin = page.find(b2search) + b2search.length();
  auto r3end = page.find("'", r3begin);
  auto r4begin = page.find('-', r3end) + 1;
  auto r4end = page.find('+', r4begin);
  auto r5begin = page.find('/', r4end) + 2;
  auto r5end = page.find('-', r5begin);

  return {page.substr(r1begin, r1end - r1begin),
          page.substr(r2begin, r2end - r2begin),
          page.substr(r3begin, r3end - r3begin),
          page.substr(r4begin, r4end - r4begin),
          page.substr(r5begin, r5end - r5begin)};
}

std::string decipher(const std::string &code,
                     const std::vector<std::string> &r) {
  if (code.empty())
    throw std::logic_error(util::Error::COULD_NOT_FIND_DECIPHER_CODE);
  auto _0x531f91 = code;
  auto _0x5d72cd = std::string(1, _0x531f91[0]);
  _0x5d72cd = _0x531f91;
  auto _0x1bf6e5 = std::string();
  auto _0x41e0ff = 9 * 8LL;
  auto _0x439a49 = _0x5d72cd.substr(0, _0x41e0ff);

  struct {
    std::string k;
    std::vector<int64_t> ke;
  } _0x31f4aa = {_0x439a49, {}};

  auto i = 0LL;
  for (; i < static_cast<int64_t>(_0x439a49.length()); i += 8) {
    auto _0x40b427 = _0x439a49.substr(i, 8);
    auto _0x577716 = std::stoll(_0x40b427, nullptr, 16);
    _0x31f4aa.ke.push_back(_0x577716);
  }

  auto _0x3d7b02 = _0x31f4aa.ke;
  _0x41e0ff = 9 * 8;
  _0x5d72cd = _0x5d72cd.substr(_0x41e0ff);
  auto _0x439a49_i = 0LL;

  auto _0x145894 = 0LL;
  for (; _0x439a49_i < static_cast<int64_t>(_0x5d72cd.length());) {
    auto _0x5eb93a = 64LL;
    auto _0x37c346 = 127LL;
    auto _0x896767 = 0LL;
    auto _0x1a873b = 0LL;
    auto _0x3d9c8e = 0LL;
    struct {
      int mm;
      int xx;
    } _0x31f4aa = {128, 63};

    do {
      if (_0x439a49_i + 1 >= static_cast<int64_t>(_0x5d72cd.length())) {
        _0x5eb93a = 143;
      }

      auto _0x1fa71e = _0x5d72cd.substr(_0x439a49_i, 2);
      _0x439a49_i++;
      _0x439a49_i++;
      _0x3d9c8e = std::stoll(_0x1fa71e, nullptr, 16);

      if (_0x1a873b < 6 * 5) {
        auto _0x332549 = _0x3d9c8e & _0x31f4aa.xx;
        _0x896767 += _0x332549 << _0x1a873b;
      } else {
        auto _0x332549 = _0x3d9c8e & _0x31f4aa.xx;
        _0x896767 += _0x332549 * (1LL << _0x1a873b);
      }
      _0x1a873b += 6;
    } while (_0x3d9c8e >= _0x5eb93a);

    auto _1x4bfb36 = std::stoll(r[0], nullptr, 8) - std::stoll(r[1]);
    auto _0x30725e = _0x896767 ^ _0x3d7b02[_0x145894 % 9];

    _0x30725e =
        (_0x30725e ^ (std::stoll(r[2], nullptr, 8) - std::stoll(r[3]) + 0x4) /
                         (std::stoll(r[4]) - 0x8)) ^
        _1x4bfb36;

    auto _0x2de433 = 2 * _0x5eb93a + _0x37c346;

    i = 0;
    for (; i < 4; i++) {
      auto _0x1a9381 = _0x30725e & _0x2de433;
      auto _0x1a0e90 = (_0x41e0ff / 9) * i;
      _0x1a9381 = _0x1a9381 >> _0x1a0e90;
      auto _0x3fa834 = std::string(1, _0x1a9381 - 1);
      if (_0x3fa834 != "$") {
        _0x1bf6e5 += _0x3fa834;
      }
      _0x2de433 = _0x2de433 << (_0x41e0ff / 9);
    }

    _0x145894 += 1;
  }
  return _0x1bf6e5;
}

std::string extract_url(const std::string &page) {
  return "https://openload.co/stream/" + decipher(cipher(page), values(page)) +
         "?mime=true";
}

}  // namespace openload

namespace mp4upload {

std::string radix(uint64_t n, uint64_t base) {
  if (base == 0 || base > 36)
    throw std::logic_error(util::Error::INVALID_RADIX_BASE);
  const std::string numerals("0123456789abcdefghijklmnopqrstuvwxyz");
  std::string result;
  while (n > 0) {
    result += numerals[n % base];
    n /= base;
  }
  if (result.empty()) {
    result = "0";
  } else {
    std::reverse(result.begin(), result.end());
  }
  return result;
}

std::string unpack_js(std::string p, uint64_t a, uint64_t c,
                      const std::vector<std::string> &k) {
  while (c > 0) {
    c -= 1;
    if (c < k.size() && (!k[c].empty())) {
      re::regex replace_rx("\\b" + radix(c, a) + "\\b");
      p = re::regex_replace(p, replace_rx, k[c]);
    }
  }
  return p;
}

std::string find_embed_url(const std::string &page) {
  const auto search = "<IFRAME SRC=\"";
  auto start = page.find(search) + strlen(search);
  auto end = page.find("\"", start);
  return std::string(page.begin() + start, page.begin() + end);
}

std::string extract_url(const std::string &page) {
  auto start = page.find("p,a,c,k");
  if (start == std::string::npos) {
    throw std::logic_error(util::Error::COULD_NOT_FIND_PACKED_SCRIPT);
  }
  start = page.find("return", start);
  if (start == std::string::npos) {
    throw std::logic_error(util::Error::COULD_NOT_FIND_PACKED_SCRIPT);
  }
  start = page.find("'", start);
  if (start == std::string::npos) {
    throw std::logic_error(util::Error::COULD_NOT_FIND_PACKED_SCRIPT);
  }
  auto end = page.find("</script>", start);
  if (end == std::string::npos) {
    throw std::logic_error(util::Error::COULD_NOT_FIND_PACKED_SCRIPT);
  }
  const std::string code = page.substr(start, end - start);
  re::smatch match;
  re::regex arg_rx("'(.*?)',([0-9]*),([0-9]*),'(.*?)'");
  if (!re::regex_search(code, match, arg_rx)) {
    throw std::logic_error(util::Error::COULD_NOT_EXTRACT_PACKED_ARGUMENTS);
  }
  std::vector<std::string> arg_k;
  std::string buffer;
  for (char c : match[4].str()) {
    if (c == '|') {
      arg_k.push_back(buffer);
      buffer.clear();
    } else {
      buffer += c;
    }
  }
  arg_k.push_back(buffer);
  uint64_t arg_a = uint64_t(std::atoll(match[2].str().c_str()));
  uint64_t arg_c = uint64_t(std::atoll(match[3].str().c_str()));
  std::string unpacked = unpack_js(match[1].str(), arg_a, arg_c, arg_k);
  re::regex source_rx("\"file\":\"([^\"]*)\"");
  re::smatch source_match;
  if (!re::regex_search(unpacked, source_match, source_rx)) {
    throw std::logic_error(util::Error::COULD_NOT_FIND_MP4_URL);
  }
  return source_match[1].str();
}

}  // namespace mp4upload

AnimeZone::AnimeZone() : CloudProvider(util::make_unique<Auth>()) {}

AuthorizeRequest::Pointer AnimeZone::authorizeAsync() {
  return util::make_unique<AuthorizeRequest>(
      shared_from_this(), [=](AuthorizeRequest::Pointer r,
                              AuthorizeRequest::AuthorizeCompleted complete) {
        r->query(
            [=](util::Output) {
              auto request = http()->create(endpoint());
              request->setHeaderParameter("User-Agent", USER_AGENT);
              return request;
            },
            [=](Response e) {
              if (!IHttpRequest::isSuccess(e.http_code())) {
                complete(Error{e.http_code(), e.error_output().str()});
              } else {
                auto session = extract_session(e.headers());
                if (session == "") {
                  complete(Error{IHttpRequest::Failure,
                                 util::Error::COULD_NOT_FIND_SESSION_TOKEN});
                } else {
                  {
                    auto lock = auth_lock();
                    auth()->access_token()->token_ = session;
                  }
                  complete(nullptr);
                }
              }
            });
      });
}

void AnimeZone::authorizeRequest(IHttpRequest &r) const {
  auto session = this->access_token();
  if (!session.empty()) r.setHeaderParameter("Cookie", "_SESS=" + session);
  r.setHeaderParameter("User-Agent", USER_AGENT);
}

bool AnimeZone::reauthorize(int code,
                            const IHttpRequest::HeaderParameters &h) const {
  return code == IHttpRequest::NotFound ||
         code == IHttpRequest::InternalServerError || extract_session(h) != "";
}

std::string AnimeZone::name() const { return "animezone"; }

std::string AnimeZone::endpoint() const { return "https://www.animezone.pl"; }

bool AnimeZone::isSuccess(int code,
                          const IHttpRequest::HeaderParameters &headers) const {
  return IHttpRequest::isSuccess(code) && extract_session(headers) == "";
}

ICloudProvider::OperationSet AnimeZone::supportedOperations() const {
  return GetItem | ListDirectoryPage | ListDirectory | DownloadFile |
         GetItemUrl;
}

ICloudProvider::GeneralDataRequest::Pointer AnimeZone::getGeneralDataAsync(
    GeneralDataCallback cb) {
  return std::make_shared<Request<EitherError<GeneralData>>>(
             shared_from_this(), cb,
             [=](Request<EitherError<GeneralData>>::Pointer r) {
               GeneralData data;
               data.space_total_ = 0;
               data.space_used_ = 0;
               r->done(data);
             })
      ->run();
}

ICloudProvider::GetItemDataRequest::Pointer AnimeZone::getItemDataAsync(
    const std::string &id, GetItemDataCallback callback) {
  return std::make_shared<Request<EitherError<IItem>>>(
             shared_from_this(), callback,
             [=](Request<EitherError<IItem>>::Pointer r) {
               if (id == rootDirectory()->id()) return r->done(rootDirectory());
               try {
                 auto json = util::json::from_string(id);
                 auto type = json["type"].asString();
                 std::string name;
                 if (type == "letter")
                   name = json["letter"].asString();
                 else if (type == "anime")
                   name = json["anime"].asString();
                 else
                   name = json["name"].asString();
                 IItem::Pointer item = util::make_unique<Item>(
                     name, id, IItem::UnknownSize, IItem::UnknownTimeStamp,
                     json.isMember("player") ? IItem::FileType::Video
                                             : IItem::FileType::Directory);
                 r->done(item);
               } catch (const Json::Exception &e) {
                 r->done(Error{IHttpRequest::Failure, e.what()});
               }
             })
      ->run();
}

ICloudProvider::DownloadFileRequest::Pointer AnimeZone::downloadFileAsync(
    IItem::Pointer i, IDownloadFileCallback::Pointer cb, Range range) {
  return std::make_shared<DownloadFileFromUrlRequest>(shared_from_this(), i, cb,
                                                      range)
      ->run();
}

IHttpRequest::Pointer AnimeZone::listDirectoryRequest(
    const IItem &directory, const std::string &page_token,
    std::ostream &) const {
  if (directory.id() == rootDirectory()->id()) {
    return http()->create(endpoint() + "/anime/lista");
  }
  auto data = util::json::from_string(directory.id());
  auto type = data["type"].asString();
  if (type == "letter") {
    auto letter = data["letter"].asString();
    const std::string url = endpoint() + "/anime/lista/" + letter;
    auto r = http()->create(url);
    if (page_token != "") {
      r->setParameter("page", page_token);
    }
    return r;
  }
  if (type == "anime") {
    auto anime_url = data["anime_url"].asString();
    return http()->create(anime_url);
  }
  if (type == "episode") {
    auto episode_url = endpoint() + data["episode_url"].asString();
    return http()->create(episode_url);
  }
  return nullptr;
}

IItem::List AnimeZone::listDirectoryResponse(const IItem &directory,
                                             std::istream &response,
                                             std::string &page_token) const {
  if (directory.id() == rootDirectory()->id()) return rootDirectoryContent();
  std::stringstream stream;
  stream << response.rdbuf();
  auto dir_data = util::json::from_string(directory.id());
  if (dir_data["type"] == "letter")
    return letterDirectoryContent(stream.str(), page_token);
  else if (dir_data["type"] == "anime")
    return animeDirectoryContent(dir_data["anime"].asString(), stream.str());
  else if (dir_data["type"] == "episode")
    return episodeDirectoryContent(
        dir_data["episode_url"].asString(), dir_data["anime"].asString(),
        dir_data["episode_no"].asString(), stream.str());
  else
    throw std::logic_error(util::Error::UNKNOWN_RESPONSE_RECEIVED);
}

ICloudProvider::GetItemUrlRequest::Pointer AnimeZone::getItemUrlAsync(
    IItem::Pointer item, GetItemUrlCallback cb) {
  auto value = util::json::from_string(item->id());
  auto fetch_player = [=](Request<EitherError<std::string>>::Pointer r,
                          const std::string &url) {
    r->send([=](util::Output) { return http()->create(url); },
            [=](EitherError<Response> e) {
              if (e.left()) {
                r->done(e.left());
              } else {
                try {
                  auto value = util::json::from_string(item->id());
                  const std::string player = value["player"].asString();
                  if (player == "openload.co") {
                    r->done(openload::extract_url(e.right()->output().str()));
                  } else if (player == "mp4upload.com") {
                    r->send(
                        [=](util::Output) {
                          return http()->create(mp4upload::find_embed_url(
                              e.right()->output().str()));
                        },
                        [=](EitherError<Response> e) {
                          if (e.left())
                            r->done(e.left());
                          else
                            r->done(mp4upload::extract_url(
                                e.right()->output().str()));
                        });
                  } else {
                    throw std::logic_error(util::Error::UNSUPPORTED_PLAYER);
                  }
                } catch (const std::exception &e) {
                  r->done(Error{IHttpRequest::Failure, e.what()});
                }
              }
            });
  };
  auto fetch_frame = [=](Request<EitherError<std::string>>::Pointer r,
                         const std::string &origin, const std::string &code) {
    r->request(
        [=](util::Output payload) {
          auto request = http()->create(origin, "POST");
          request->setHeaderParameter(
              "Content-Type",
              "application/x-www-form-urlencoded; charset=UTF-8");
          request->setHeaderParameter("Referer", origin);
          (*payload) << "data=" + code;
          return request;
        },
        [=](EitherError<Response> e) {
          if (e.left()) {
            r->done(e.left());
          } else {
            re::regex src_rx("(?:src|href)=\"([^\"]*)\"", re::regex::icase);
            re::smatch match;
            std::string content = e.right()->output().str();
            if (!re::regex_search(content, match, src_rx))
              r->done(
                  Error{IHttpRequest::Failure, "Source not found in frame."});
            else
              fetch_player(r, match[1].str());
          }
        });
  };
  auto fetch_player_list = [=](Request<EitherError<std::string>>::Pointer r) {
    r->request(
        [=](util::Output) {
          return http()->create(value["origin"].asString());
        },
        [=](EitherError<Response> e) {
          if (e.left())
            r->done(e.left());
          else {
            try {
              auto episodes = episode_to_players(e.right()->output().str());
              fetch_frame(r, value["origin"].asString(),
                          episodes[value["idx"].asUInt()].code_);
            } catch (const std::exception &e) {
              r->done(Error{IHttpRequest::Failure, e.what()});
            }
          }
        });
  };
  auto authorize_session = [=](Request<EitherError<std::string>>::Pointer r) {
    r->request(
        [=](util::Output) {
          return http()->create(endpoint() + "/images/statistics.gif");
        },
        [=](EitherError<Response> e) {
          if (e.left()) {
            r->done(e.left());
          } else {
            fetch_player_list(r);
          }
        });
  };
  return std::make_shared<Request<EitherError<std::string>>>(
             shared_from_this(), cb, authorize_session)
      ->run();
}

IItem::List AnimeZone::rootDirectoryContent() const {
  IItem::List result;
  std::vector<char> letters;
  letters.push_back('0');
  for (char c = 'A'; c <= 'Z'; ++c) {
    letters.push_back(c);
  }
  for (auto l : letters) {
    Json::Value value;
    value["type"] = "letter";
    value["letter"] = std::string() + l;
    result.push_back(util::make_unique<Item>(
        std::string() + l, util::json::to_string(value), IItem::UnknownSize,
        IItem::UnknownTimeStamp, IItem::FileType::Directory));
  }
  return result;
}

IItem::List AnimeZone::letterDirectoryContent(
    const std::string &content, std::string &next_page_token) const {
  IItem::List result;
  re::regex anime_rx("<a href=\"(/odcinki/[^\"]*)\">([^<]*)</a>");
  for (auto it = re::sregex_iterator(content.begin(), content.end(), anime_rx);
       it != re::sregex_iterator(); ++it) {
    Json::Value value;
    const auto anime = (*it)[2].str();
    const auto anime_url = (*it)[1].str();
    value["type"] = "anime";
    value["anime_url"] = endpoint() + anime_url;
    value["anime"] = anime;
    result.push_back(util::make_unique<Item>(
        anime, util::json::to_string(value), IItem::UnknownSize,
        IItem::UnknownTimeStamp, IItem::FileType::Directory));
  }
  re::regex next_rx("<a href=\"/anime/lista/[^=]*=([0-9]*)\">&raquo;</a>");
  re::smatch next_match;
  if (re::regex_search(content, next_match, next_rx)) {
    next_page_token = next_match[1].str();
  }
  return result;
}

IItem::List AnimeZone::animeDirectoryContent(const std::string &anime_name,
                                             const std::string &content) const {
  IItem::List result;
  re::regex episode_rx(
      "<strong>([^<]*)</"
      "strong>[^\n]*\n[^\"]*\"episode-title\">([^<]*)<(?:[^\n]*\n){3}.*?<a "
      "href=\"..(/odcinek[^\"]*)\"");
  auto list_start = content.find("</thead>");
  if (list_start == std::string::npos) {
    return result;
  }
  for (auto it = re::sregex_iterator(
           content.begin() + static_cast<int64_t>(list_start), content.end(),
           episode_rx);
       it != re::sregex_iterator(); ++it) {
    Json::Value value;
    const auto episode_no = (*it)[1].str();
    const auto episode_title = (*it)[2].str();
    const auto episode_url = (*it)[3].str();
    value["type"] = "episode";
    value["episode_no"] = episode_no;
    value["episode_title"] = episode_title;
    value["episode_url"] = episode_url;
    value["anime"] = anime_name;
    std::string name = episode_no;
    if (episode_title != "" && episode_title != " ") {
      name += ": " + episode_title;
    }
    value["name"] = name;
    result.push_back(util::make_unique<Item>(
        name, util::json::to_string(value), IItem::UnknownSize,
        IItem::UnknownTimeStamp, IItem::FileType::Directory));
  }
  return result;
}

IItem::List AnimeZone::episodeDirectoryContent(
    const std::string &episode_url, const std::string &anime_name,
    const std::string &episode_no, const std::string &content) const {
  auto players = episode_to_players(content);
  const auto origin = endpoint() + episode_url;
  std::unordered_map<std::string, uint64_t> player_counter;
  const std::unordered_set<std::string> supported_players = {"openload.co",
                                                             "mp4upload.com"};
  auto idx = 0;
  IItem::List result;
  for (const auto &player : players) {
    const std::string lower_player_name = util::to_lower(player.name_);
    if (supported_players.find(lower_player_name) == supported_players.end()) {
      idx++;
      continue;
    }
    Json::Value value;
    value["idx"] = idx++;
    value["origin"] = origin;
    value["player"] = lower_player_name;
    const std::string counter_key = player.name_ + ":" + player.language_;
    const uint64_t player_index = player_counter[counter_key];
    player_counter[counter_key] += 1;
    const std::vector<std::string> name_segments = {
        anime_name, episode_no, "[" + player.language_ + "]",
        "[" + player.name_ + "]"};
    std::string player_name;
    const std::string name_sep = " ";
    bool first = true;
    for (const auto &segment : name_segments) {
      if (!first) {
        player_name += name_sep;
      } else {
        first = false;
      }
      player_name += segment;
    }
    if (player_index > 0) {
      player_name += "(" + std::to_string(player_index) + ")";
    }
    player_name += ".mp4";
    value["name"] = player_name;
    result.push_back(util::make_unique<Item>(
        player_name, util::json::to_string(value), IItem::UnknownSize,
        IItem::UnknownTimeStamp, IItem::FileType::Video));
  }
  return result;
}

std::string AnimeZone::Auth::authorizeLibraryUrl() const {
  return redirect_uri() + "/login?state=" + state();
}

IHttpRequest::Pointer AnimeZone::Auth::exchangeAuthorizationCodeRequest(
    std::ostream &) const {
  return nullptr;
}

IHttpRequest::Pointer AnimeZone::Auth::refreshTokenRequest(
    std::ostream &) const {
  return nullptr;
}

IAuth::Token::Pointer AnimeZone::Auth::exchangeAuthorizationCodeResponse(
    std::istream &) const {
  return nullptr;
}

IAuth::Token::Pointer AnimeZone::Auth::refreshTokenResponse(
    std::istream &) const {
  return nullptr;
}

}  // namespace cloudstorage
