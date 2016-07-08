/*****************************************************************************
 * AmazonDrive.h : AmazonDrive headers
 *
 *****************************************************************************
 * Copyright (C) 2016-2016 VideoLAN
 *
 * Authors: Paweł Wegner <pawel.wegner95@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef AMAZONDRIVE_H
#define AMAZONDRIVE_H

#include "CloudProvider.h"

namespace cloudstorage {

class AmazonDrive : public CloudProvider {
 public:
  AmazonDrive();
  std::string name() const;
  IItem::Pointer rootDirectory() const;

 protected:
  cloudstorage::AuthorizeRequest::Pointer authorizeAsync();

 private:
  HttpRequest::Pointer listDirectoryRequest(const IItem&,
                                            std::ostream& input_stream) const;
  HttpRequest::Pointer uploadFileRequest(const IItem& directory,
                                         const std::string& filename,
                                         std::istream& stream,
                                         std::ostream& input_stream) const;
  HttpRequest::Pointer downloadFileRequest(const IItem&,
                                           std::ostream& input_stream) const;
  HttpRequest::Pointer getThumbnailRequest(const IItem&,
                                           std::ostream& input_stream) const;

  std::vector<IItem::Pointer> listDirectoryResponse(
      std::istream&, HttpRequest::Pointer& next_page_request,
      std::ostream&) const;

  IItem::FileType kindToType(const std::string& type) const;

  class Auth : public cloudstorage::Auth {
   public:
    Auth();

    std::string authorizeLibraryUrl() const;

    HttpRequest::Pointer exchangeAuthorizationCodeRequest(
        std::ostream& input_data) const;
    HttpRequest::Pointer refreshTokenRequest(std::ostream& input_data) const;
    HttpRequest::Pointer validateTokenRequest(std::ostream& input_data) const;

    Token::Pointer exchangeAuthorizationCodeResponse(std::istream&) const;
    Token::Pointer refreshTokenResponse(std::istream&) const;
    bool validateTokenResponse(std::istream&) const;
  };

  class AuthorizeRequest : public cloudstorage::AuthorizeRequest {
   public:
    using cloudstorage::AuthorizeRequest::AuthorizeRequest;

   protected:
    bool authorize();
  };

  std::string metadata_url_;
  std::string content_url_;
  std::string root_id_;
};

}  // namespace cloudstorage

#endif  // AMAZONDRIVE_H
