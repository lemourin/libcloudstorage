/*****************************************************************************
 * AmazonDrive.h : AmazonDrive headers
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

#ifndef AMAZONDRIVE_H
#define AMAZONDRIVE_H

#include "CloudProvider.h"
#include "Utility/Item.h"

namespace cloudstorage {

class AmazonDrive : public CloudProvider {
 public:
  AmazonDrive();

  void initialize(const std::string& token, ICallback::Pointer,
                  const Hints& hints);

  Hints hints() const;
  std::string name() const;
  IItem::Pointer rootDirectory() const;

 protected:
  cloudstorage::AuthorizeRequest::Pointer authorizeAsync();

 private:
  HttpRequest::Pointer getItemDataRequest(const std::string& id,
                                          std::ostream& input_stream) const;
  HttpRequest::Pointer listDirectoryRequest(const IItem&,
                                            const std::string& page_token,
                                            std::ostream& input_stream) const;
  HttpRequest::Pointer uploadFileRequest(const IItem& directory,
                                         const std::string& filename,
                                         std::istream& stream,
                                         std::ostream& input_stream) const;
  HttpRequest::Pointer downloadFileRequest(const IItem&,
                                           std::ostream& input_stream) const;
  HttpRequest::Pointer deleteItemRequest(const IItem&,
                                         std::ostream& input_stream) const;

  IItem::Pointer getItemDataResponse(std::istream& response) const;
  std::vector<IItem::Pointer> listDirectoryResponse(
      std::istream&, std::string& page_token) const;

  IItem::FileType type(const Json::Value&) const;
  IItem::Pointer toItem(const Json::Value&) const;

  std::string metadata_url() const;
  std::string content_url() const;

  bool reauthorize(int code) const;

  class AmazonItem : public Item {
   public:
    using Item::Item;

    std::string parent_;
  };

  class Auth : public cloudstorage::Auth {
   public:
    Auth();

    std::string authorizeLibraryUrl() const;

    HttpRequest::Pointer exchangeAuthorizationCodeRequest(
        std::ostream& input_data) const;
    HttpRequest::Pointer refreshTokenRequest(std::ostream& input_data) const;

    Token::Pointer exchangeAuthorizationCodeResponse(std::istream&) const;
    Token::Pointer refreshTokenResponse(std::istream&) const;
  };

  std::string metadata_url_;
  std::string content_url_;
};

}  // namespace cloudstorage

#endif  // AMAZONDRIVE_H
