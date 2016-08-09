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

#include <json/forwards.h>

#include "CloudProvider.h"
#include "Utility/Item.h"

namespace cloudstorage {

class AmazonDrive : public CloudProvider {
 public:
  AmazonDrive();

  void initialize(InitData&&) override;

  Hints hints() const override;
  std::string name() const override;
  std::string endpoint() const override;
  IItem::Pointer rootDirectory() const override;

  MoveItemRequest::Pointer moveItemAsync(IItem::Pointer source,
                                         IItem::Pointer destination,
                                         MoveItemCallback) override;
  cloudstorage::AuthorizeRequest::Pointer authorizeAsync() override;

 private:
  IHttpRequest::Pointer getItemDataRequest(
      const std::string& id, std::ostream& input_stream) const override;
  IHttpRequest::Pointer listDirectoryRequest(
      const IItem&, const std::string& page_token,
      std::ostream& input_stream) const override;
  IHttpRequest::Pointer uploadFileRequest(const IItem& directory,
                                          const std::string& filename,
                                          std::ostream&,
                                          std::ostream&) const override;
  IHttpRequest::Pointer downloadFileRequest(
      const IItem&, std::ostream& input_stream) const override;
  IHttpRequest::Pointer deleteItemRequest(
      const IItem&, std::ostream& input_stream) const override;
  IHttpRequest::Pointer createDirectoryRequest(const IItem&, const std::string&,
                                               std::ostream&) const override;
  IHttpRequest::Pointer renameItemRequest(const IItem&, const std::string& name,
                                          std::ostream&) const override;

  IItem::Pointer getItemDataResponse(std::istream& response) const override;
  std::vector<IItem::Pointer> listDirectoryResponse(
      std::istream&, std::string& page_token) const override;

  IItem::FileType type(const Json::Value&) const;
  IItem::Pointer toItem(const Json::Value&) const;

  std::string metadata_url() const;
  std::string content_url() const;

  bool reauthorize(int code) const override;

  class Auth : public cloudstorage::Auth {
   public:
    Auth();

    std::string authorizeLibraryUrl() const override;

    IHttpRequest::Pointer exchangeAuthorizationCodeRequest(
        std::ostream& input_data) const override;
    IHttpRequest::Pointer refreshTokenRequest(
        std::ostream& input_data) const override;

    Token::Pointer exchangeAuthorizationCodeResponse(
        std::istream&) const override;
    Token::Pointer refreshTokenResponse(std::istream&) const override;
  };

  std::string metadata_url_;
  std::string content_url_;
};

}  // namespace cloudstorage

#endif  // AMAZONDRIVE_H
