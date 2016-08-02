/*****************************************************************************
 * AmazonS3.h : AmazonS3 headers
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

#ifndef AMAZONS3_H
#define AMAZONS3_H

#include "CloudProvider.h"

#include "Utility/Item.h"

namespace cloudstorage {

class AmazonS3 : public CloudProvider {
 public:
  AmazonS3();

  void initialize(const std::string& token, ICallback::Pointer,
                  const Hints& hints);

  std::string token() const;
  std::string name() const;

  AuthorizeRequest::Pointer authorizeAsync();

  HttpRequest::Pointer getItemDataRequest(const std::string&,
                                          std::ostream& input_stream) const;
  HttpRequest::Pointer listDirectoryRequest(const IItem&,
                                            const std::string& page_token,
                                            std::ostream& input_stream) const;
  HttpRequest::Pointer uploadFileRequest(const IItem& directory,
                                         const std::string& filename,
                                         std::ostream& prefix_stream,
                                         std::ostream& suffix_stream) const;
  HttpRequest::Pointer downloadFileRequest(const IItem&,
                                           std::ostream& input_stream) const;
  HttpRequest::Pointer deleteItemRequest(const IItem&,
                                         std::ostream& input_stream) const;
  HttpRequest::Pointer createDirectoryRequest(const IItem&,
                                              const std::string& name,
                                              std::ostream&) const;
  HttpRequest::Pointer moveItemRequest(const IItem&, const IItem&,
                                       std::ostream&) const;
  HttpRequest::Pointer renameItemRequest(const IItem&, const std::string& name,
                                         std::ostream&) const;

  IItem::Pointer getItemDataResponse(std::istream& response) const;
  std::vector<IItem::Pointer> listDirectoryResponse(
      std::istream&, std::string& next_page_token) const;

  void authorizeRequest(HttpRequest&) const;
  bool reauthorize(int) const;

  std::string access_id() const;
  std::string secret() const;

  struct S3Item : public Item {
    using Item::Item;

    std::string bucket_;
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

 private:
  std::string access_id_;
  std::string secret_;
};

}  // namespace cloudstorage

#endif  // AMAZONS3_H