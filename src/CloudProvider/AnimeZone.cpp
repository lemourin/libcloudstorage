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
#include "Request/DownloadFileRequest.h"
#include "Request/Request.h"
#include "Utility/Item.h"
#include "Utility/Utility.h"

namespace cloudstorage {

namespace {

void work(Request<EitherError<IItem::List>>::Pointer r,
          std::shared_ptr<IItem::List> result, IItem::Pointer directory,
          std::string page_token, IListDirectoryCallback *callback) {
  r->request(
      [=](util::Output i) {
        return r->provider()->listDirectoryRequest(*directory, page_token, *i);
      },
      [=](EitherError<Response> e) {
        if (e.left()) return r->done(e.left());
        try {
          std::string page_token = "";
          for (auto &t :
               static_cast<AnimeZone *>(r->provider().get())
                   ->listDirectoryResponse(*directory, e.right()->output(),
                                           e.right()->headers(), page_token)) {
            callback->receivedItem(t);
            result->push_back(t);
          }
          if (!page_token.empty())
            work(r, result, directory, page_token, callback);
          else {
            r->done(*result);
          }
        } catch (const std::exception &) {
          r->done(Error{IHttpRequest::Failure, e.right()->output().str()});
        }
      });
}

std::vector<std::pair<std::string, std::string>> episode_to_players(
    const std::string &page) {
  std::vector<std::pair<std::string, std::string>> result;
  auto start = page.find("Wszystkie odcinki");
  if (start == std::string::npos) {
    throw std::logic_error("Players not found.");
  }
  std::regex buttonRX(
      "<td>(.*?)</td>(?:.|\\r?\\n)*?button "
      "class=.*play.*data-[^=]*=\"([^\"]*)\"",
      std::regex_constants::ECMAScript);
  for (auto it = std::sregex_iterator(
           page.begin() + static_cast<ssize_t>(start), page.end(), buttonRX);
       it != std::sregex_iterator(); ++it) {
    result.push_back({(*it)[1].str(), (*it)[2].str()});
  }
  return result;
}

}  // namespace

namespace openload {

std::string cipher(const std::string &page) {
  std::string cipheredBegin =
      "<div style=\"display:none;\">\n<span style=\"\" id=\"";
  auto begin =
      page.find('>', page.find(cipheredBegin) + cipheredBegin.length()) + 1;
  auto end = page.find('<', begin) - 1;
  return page.substr(begin, end - begin + 1);
}

std::vector<std::string> values(const std::string &page) {
  std::string b1search = "var _1x4bfb36=parseInt('";
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
    _0x41e0ff = i * 8;
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

    _0x30725e = (_0x30725e ^
                 (std::stoll(r[2], nullptr, 8) - std::stoll(r[3]) + 0x4) /
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

AnimeZone::AnimeZone() : CloudProvider(util::make_unique<Auth>()) {}

AuthorizeRequest::Pointer AnimeZone::authorizeAsync() {
  return util::make_unique<AuthorizeRequest>(
      shared_from_this(), [=](AuthorizeRequest::Pointer,
                              AuthorizeRequest::AuthorizeCompleted complete) {
        complete(nullptr);
      });
}

std::string AnimeZone::name() const { return "animezone"; }

std::string AnimeZone::endpoint() const { return "http://www.animezone.pl"; }

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

ICloudProvider::DownloadFileRequest::Pointer AnimeZone::downloadFileAsync(
    IItem::Pointer i, IDownloadFileCallback::Pointer cb, Range range) {
  return std::make_shared<DownloadFileFromUrlRequest>(shared_from_this(), i, cb,
                                                      range)
      ->run();
}

ICloudProvider::ListDirectoryRequest::Pointer AnimeZone::listDirectoryAsync(
    IItem::Pointer directory, IListDirectoryCallback::Pointer callback) {
  return std::make_shared<Request<EitherError<IItem::List>>>(
             shared_from_this(),
             [=](EitherError<IItem::List> e) { callback->done(e); },
             [=](Request<EitherError<IItem::List>>::Pointer r) {
               work(r, std::make_shared<IItem::List>(), directory, "",
                    callback.get());
             })
      ->run();
}

ICloudProvider::ListDirectoryPageRequest::Pointer
AnimeZone::listDirectoryPageAsync(IItem::Pointer directory,
                                  const std::string &token,
                                  ListDirectoryPageCallback cb) {
  return std::make_shared<Request<EitherError<PageData>>>(
             shared_from_this(), cb,
             [=](Request<EitherError<PageData>>::Pointer r) {
               r->request(
                   [=](util::Output input) {
                     return r->provider()->listDirectoryRequest(*directory,
                                                                token, *input);
                   },
                   [=](EitherError<Response> e) {
                     if (e.left()) return r->done(e.left());
                     try {
                       std::string next_token;
                       auto lst = listDirectoryResponse(
                           *directory, e.right()->output(),
                           e.right()->headers(), next_token);
                       r->done(PageData{lst, next_token});
                     } catch (const std::exception &) {
                       r->done(Error{IHttpRequest::Failure,
                                     e.right()->output().str()});
                     }
                   });
             })
      ->run();
}

IHttpRequest::Pointer AnimeZone::listDirectoryRequest(
    const IItem &directory, const std::string &page_token,
    std::ostream &) const {
  if (directory.id() == rootDirectory()->id()) {
    return http()->create("http://www.animezone.pl/anime/lista");
  }
  auto data = util::json::from_string(directory.id());
  auto type = data["type"].asString();
  if (type == "letter") {
    auto letter = data["letter"].asString();
    const std::string url = "http://www.animezone.pl/anime/lista/" + letter;
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

IItem::List AnimeZone::listDirectoryResponse(
    const IItem &directory, std::istream &response,
    const IHttpRequest::HeaderParameters &headers,
    std::string &page_token) const {
  IItem::List result;
  if (directory.id() == rootDirectory()->id()) {
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
  auto value = util::json::from_string(directory.id());
  if (value["type"] == "letter") {
    auto letter = value["letter"].asString();
    std::string content;
    {
      std::stringstream stream;
      stream << response.rdbuf();
      content = stream.str();
    }
    std::regex anime_rx("<a href=\"(/odcinki/[^\"]*)\">(.*?)</a>",
                        std::regex::ECMAScript);
    for (auto it =
             std::sregex_iterator(content.begin(), content.end(), anime_rx);
         it != std::sregex_iterator(); ++it) {
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
    std::regex next_rx("<a href=\"/anime/lista/[^=]*=([0-9]*)\">&raquo;</a>",
                       std::regex::ECMAScript);
    std::smatch next_match;
    if (std::regex_search(content, next_match, next_rx)) {
      page_token = next_match[1].str();
    }
    return result;
  }
  if (value["type"] == "anime") {
    std::string content;
    {
      std::stringstream stream;
      stream << response.rdbuf();
      content = stream.str();
    }
    std::regex episode_rx(
        "<strong>([^<]*)</"
        "strong>(?:.|\r?\n)*?\"episode-title\">([^<]*)<(?:.|\r?\n)*?<a "
        "href=\"..(/odcinek[^\"]*)\"",
        std::regex::ECMAScript);
    auto list_start = content.find("</thead>");
    if (list_start == std::string::npos) {
      throw std::logic_error(
          "Could not find the beginning of the episodes list.");
    }
    for (auto it = std::sregex_iterator(content.begin() + ssize_t(list_start),
                                        content.end(), episode_rx);
         it != std::sregex_iterator(); ++it) {
      Json::Value value;
      const auto episode_no = (*it)[1].str();
      const auto episode_title = (*it)[2].str();
      const auto episode_url = (*it)[3].str();
      value["type"] = "episode";
      value["episode_no"] = episode_no;
      value["episode_title"] = episode_title;
      value["episode_url"] = episode_url;
      std::string name = episode_no;
      if (episode_title != "" && episode_title != " ") {
        name += ": " + episode_title;
      }
      result.push_back(util::make_unique<Item>(
          name, util::json::to_string(value), IItem::UnknownSize,
          IItem::UnknownTimeStamp, IItem::FileType::Directory));
    }
    return result;
  }
  if (value["type"] == "episode") {
    auto cookie_range = headers.equal_range("set-cookie");
    std::string session;
    for (auto it = cookie_range.first; it != cookie_range.second; ++it) {
      std::regex sess_rx("_SESS=([^;]*);");
      std::smatch match;
      if (std::regex_search(it->second, match, sess_rx)) {
        session = match[1].str();
      }
    }
    if (session == "") {
      throw std::logic_error("Session token not found.");
    }
    std::string content;
    {
      std::stringstream stream;
      stream << response.rdbuf();
      content = stream.str();
    }
    auto players = episode_to_players(content);
    auto value = util::json::from_string(directory.id());
    const auto origin = endpoint() + value["episode_url"].asString();
    for (const auto &kv : players) {
      Json::Value value;
      value["code"] = kv.second;
      value["session"] = session;
      value["origin"] = origin;
      result.push_back(util::make_unique<Item>(
          kv.first, util::json::to_string(value), IItem::UnknownSize,
          IItem::UnknownTimeStamp, IItem::FileType::Video));
    }
    return result;
  }
  throw std::logic_error("Unknown response received.");
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
                  r->done(openload::extract_url(e.right()->output().str()));
                } catch (const std::exception &e) {
                  r->done(Error{IHttpRequest::Failure, e.what()});
                }
              }
            });
  };
  auto fetch_frame = [=](Request<EitherError<std::string>>::Pointer r) {
    r->send(
        [=](util::Output payload) {
          const auto url = value["origin"].asString();
          auto request = http()->create(url, "POST");
          request->setHeaderParameter("Cookie",
                                      "_SESS=" + value["session"].asString());
          request->setHeaderParameter(
              "Content-Type",
              "application/x-www-form-urlencoded; charset=UTF-8");
          request->setHeaderParameter(
              "User-Agent",
              "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
              "AppleWebKit/537.36 (KHTML, like Gecko) "
              "Chrome/64.0.3282.167 Safari/537.36");
          request->setHeaderParameter("Referer", url);
          request->setHeaderParameter("Accept-Encoding", "gzip, deflate");
          (*payload) << "data=" + value["code"].asString();
          return request;
        },
        [=](EitherError<Response> e) {
          if (e.left()) {
            r->done(e.left());
          } else {
            std::regex src_rx("src=\"([^\"]*)\"",
                              std::regex::ECMAScript | std::regex::icase);
            std::smatch match;
            std::string content = e.right()->output().str();
            if (!std::regex_search(content, match, src_rx))
              r->done(
                  Error{IHttpRequest::Failure, "Source not found in frame."});
            else
              fetch_player(r, match[1].str());
          }
        });
  };
  return std::make_shared<Request<EitherError<std::string>>>(
             shared_from_this(), cb,
             [=](Request<EitherError<std::string>>::Pointer r) {
               r->send(
                   [=](util::Output) {
                     const auto url =
                         "http://www.animezone.pl/images/statistics.gif";
                     auto request = http()->create(url);
                     request->setHeaderParameter(
                         "Cookie", "_SESS=" + value["session"].asString());
                     request->setHeaderParameter(
                         "User-Agent",
                         "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
                         "AppleWebKit/537.36 (KHTML, like Gecko) "
                         "Chrome/64.0.3282.167 Safari/537.36");
                     return request;
                   },
                   [=](EitherError<Response> e) {
                     if (e.left()) {
                       r->done(e.left());
                     } else {
                       fetch_frame(r);
                     }
                   });
             })
      ->run();
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
