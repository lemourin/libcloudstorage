/*****************************************************************************
 * PCloud.cpp
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
#include "PCloud.h"

namespace cloudstorage {

std::string PCloud::name() const { return "pcloud"; }
std::string PCloud::endpoint() const { return ""; }

IHttpRequest::Pointer PCloud::getItemDataRequest(const std::string&,
                                                 std::ostream&) const {
  return nullptr;
}

IHttpRequest::Pointer PCloud::listDirectoryRequest(const IItem&,
                                                   const std::string&,
                                                   std::ostream&) const {
  return nullptr;
}

IHttpRequest::Pointer PCloud::uploadFileRequest(const IItem&,
                                                const std::string&,
                                                std::ostream&,
                                                std::ostream&) const {
  return nullptr;
}

IHttpRequest::Pointer PCloud::downloadFileRequest(const IItem&,
                                                  std::ostream&) const {
  return nullptr;
}

IHttpRequest::Pointer PCloud::deleteItemRequest(const IItem&,
                                                std::ostream&) const {
  return nullptr;
}

IHttpRequest::Pointer PCloud::createDirectoryRequest(const IItem&,
                                                     const std::string&,
                                                     std::ostream&) const {
  return nullptr;
}

IHttpRequest::Pointer PCloud::moveItemRequest(const IItem&, const IItem&,
                                              std::ostream&) const {
  return nullptr;
}

IHttpRequest::Pointer PCloud::renameItemRequest(const IItem&,
                                                const std::string&,
                                                std::ostream&) const {
  return nullptr;
}

IItem::Pointer PCloud::getItemDataResponse(std::istream&) const {
  return nullptr;
}

std::vector<IItem::Pointer> PCloud::listDirectoryResponse(const IItem&,
                                                          std::istream&,
                                                          std::string&) const {
  return {};
}

IItem::Pointer PCloud::toItem(const Json::Value&) const { return nullptr; }

PCloud::Auth::Auth() {}

std::string PCloud::Auth::authorizeLibraryUrl() const { return ""; }

IHttpRequest::Pointer PCloud::Auth::exchangeAuthorizationCodeRequest(
    std::ostream&) const {
  return nullptr;
}

IHttpRequest::Pointer PCloud::Auth::refreshTokenRequest(std::ostream&) const {
  return nullptr;
}

IAuth::Token::Pointer PCloud::Auth::exchangeAuthorizationCodeResponse(
    std::istream&) const {
  return nullptr;
}

IAuth::Token::Pointer PCloud::Auth::refreshTokenResponse(std::istream&) const {
  return nullptr;
}

}  // namespace cloudstorage
