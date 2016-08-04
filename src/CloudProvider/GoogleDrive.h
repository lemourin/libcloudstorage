/*****************************************************************************
 * GoogleDrive.h : prototypes for GoogleDrive
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

#ifndef GOOGLEDRIVE_H
#define GOOGLEDRIVE_H

#include <json/forwards.h>

#include "CloudProvider.h"
#include "Utility/Auth.h"

namespace cloudstorage {

class GoogleDrive : public CloudProvider {
 public:
  GoogleDrive();
  std::string name() const;

  IHttpRequest::Pointer getItemDataRequest(const std::string&,
                                           std::ostream& input_stream) const;
  IHttpRequest::Pointer listDirectoryRequest(const IItem&,
                                             const std::string& page_token,
                                             std::ostream& input_stream) const;
  IHttpRequest::Pointer uploadFileRequest(const IItem& directory,
                                          const std::string& filename,
                                          std::ostream& prefix_stream,
                                          std::ostream& suffix_stream) const;
  IHttpRequest::Pointer downloadFileRequest(const IItem&,
                                            std::ostream& input_stream) const;
  IHttpRequest::Pointer deleteItemRequest(const IItem&,
                                          std::ostream& input_stream) const;
  IHttpRequest::Pointer createDirectoryRequest(const IItem&,
                                               const std::string& name,
                                               std::ostream&) const;
  IHttpRequest::Pointer moveItemRequest(const IItem&, const IItem&,
                                        std::ostream&) const;
  IHttpRequest::Pointer renameItemRequest(const IItem&, const std::string& name,
                                          std::ostream&) const;

  IItem::Pointer getItemDataResponse(std::istream& response) const;
  std::vector<IItem::Pointer> listDirectoryResponse(
      std::istream&, std::string& next_page_token) const;

  bool isGoogleMimeType(const std::string& mime_type) const;
  IItem::FileType toFileType(const std::string& mime_type) const;
  IItem::Pointer toItem(const Json::Value&) const;

  class Auth : public cloudstorage::Auth {
   public:
    Auth();

    std::string authorizeLibraryUrl() const;

    IHttpRequest::Pointer exchangeAuthorizationCodeRequest(
        std::ostream& input_data) const;
    IHttpRequest::Pointer refreshTokenRequest(std::ostream& input_data) const;

    Token::Pointer exchangeAuthorizationCodeResponse(std::istream&) const;
    Token::Pointer refreshTokenResponse(std::istream&) const;
  };
};

}  // namespace cloudstorage

#endif  // GOOGLEDRIVE_H
