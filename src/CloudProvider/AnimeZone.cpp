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

#include "Request/Request.h"
#include "Utility/Item.h"

namespace cloudstorage {

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
  for (; i < _0x439a49.length(); i += 8) {
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
  for (; _0x439a49_i < _0x5d72cd.length();) {
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
      if (_0x439a49_i + 1 >= _0x5d72cd.length()) {
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

std::string AnimeZone::endpoint() const { return "http://animezone.pl"; }

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

IHttpRequest::Pointer AnimeZone::listDirectoryRequest(
    const IItem &directory, const std::string &page_token,
    std::ostream &input_stream) const {
  return http()->create("https://google.com");
}

IItem::List AnimeZone::listDirectoryResponse(const IItem &directory,
                                             std::istream &response,
                                             std::string &) const {
  IItem::List result;
  result.push_back(util::make_unique<Item>(
      "best animu episode 1.mp4", "https://openload.co/embed/ka8Gt1QPYN4",
      IItem::UnknownSize, IItem::UnknownTimeStamp, IItem::FileType::Video));
  return result;
}

ICloudProvider::GetItemUrlRequest::Pointer AnimeZone::getItemUrlAsync(
    IItem::Pointer item, GetItemUrlCallback cb) {
  return std::make_shared<Request<EitherError<std::string>>>(
             shared_from_this(), cb,
             [=](Request<EitherError<std::string>>::Pointer r) {
               r->send(
                   [=](util::Output) {
                     auto request = http()->create(item->id());
                     return request;
                   },
                   [=](EitherError<Response> e) {
                     if (e.left())
                       r->done(e.left());
                     else
                       r->done(
                           openload::extract_url(e.right()->output().str()));
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
