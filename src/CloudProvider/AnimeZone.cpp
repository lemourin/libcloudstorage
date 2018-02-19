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

IItem::List AnimeZone::listDirectoryResponse(
    const IItem &directory, std::istream &response,
    std::string &next_page_token) const {
  IItem::List result;
  if (directory.id() == rootDirectory()->id()) {
    for (int i = 0; i < 10; i++)
      result.push_back(util::make_unique<Item>(
          "anime " + std::to_string(i), "some id " + std::to_string(i),
          IItem::UnknownSize, IItem::UnknownTimeStamp,
          IItem::FileType::Directory));
  } else {
    result.push_back(util::make_unique<Item>(
        "best animu episode 1.mp4", "some id", IItem::UnknownSize,
        IItem::UnknownTimeStamp, IItem::FileType::Video));
  }
  util::log(result.size());
  return result;
}

ICloudProvider::GetItemUrlRequest::Pointer AnimeZone::getItemUrlAsync(
    IItem::Pointer item, GetItemUrlCallback cb) {
  return std::make_shared<Request<EitherError<std::string>>>(
             shared_from_this(), cb,
             [=](Request<EitherError<std::string>>::Pointer r) {
               r->send(
                   [=](util::Output) {
                     auto request = http()->create("http://google.com");
                     return request;
                   },
                   [=](EitherError<Response>) {
                     r->done(std::string("http://google.com"));
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
