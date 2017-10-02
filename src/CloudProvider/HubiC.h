/*****************************************************************************
 * HubiC.cpp
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
#ifndef HUBIC_H
#define HUBIC_H

#include <json/json.h>
#include "CloudProvider.h"

namespace cloudstorage {

class HubiC : public CloudProvider {
 public:
  HubiC();

  std::string name() const override;
  std::string endpoint() const override;
  void authorizeRequest(IHttpRequest& request) const override;
  IItem::Pointer rootDirectory() const override;

  bool reauthorize(int code,
                   const IHttpRequest::HeaderParameters&) const override;
  AuthorizeRequest::Pointer authorizeAsync() override;
  MoveItemRequest::Pointer moveItemAsync(IItem::Pointer source,
                                         IItem::Pointer destination,
                                         MoveItemCallback) override;
  RenameItemRequest::Pointer renameItemAsync(IItem::Pointer item,
                                             const std::string&,
                                             RenameItemCallback) override;
  DeleteItemRequest::Pointer deleteItemAsync(IItem::Pointer,
                                             DeleteItemCallback) override;

  IHttpRequest::Pointer getItemDataRequest(
      const std::string&, std::ostream& input_stream) const override;
  IHttpRequest::Pointer getItemUrlRequest(
      const IItem&, std::ostream& input_stream) const override;
  IHttpRequest::Pointer listDirectoryRequest(
      const IItem&, const std::string& page_token,
      std::ostream& input_stream) const override;
  IHttpRequest::Pointer uploadFileRequest(
      const IItem& directory, const std::string& filename,
      std::ostream& prefix_stream, std::ostream& suffix_stream) const override;
  IHttpRequest::Pointer downloadFileRequest(
      const IItem&, std::ostream& input_stream) const override;
  IHttpRequest::Pointer createDirectoryRequest(const IItem&,
                                               const std::string& name,
                                               std::ostream&) const override;
  IItem::Pointer getItemDataResponse(std::istream& response) const override;
  IItem::Pointer uploadFileResponse(const IItem& item,
                                    const std::string& filename, uint64_t size,
                                    std::istream&) const override;
  IItem::Pointer createDirectoryResponse(const IItem& parent,
                                         const std::string& name,
                                         std::istream&) const override;
  std::string getItemUrlResponse(const IItem& item,
                                 const IHttpRequest::HeaderParameters&,
                                 std::istream& response) const override;
  std::vector<IItem::Pointer> listDirectoryResponse(
      const IItem&, std::istream&, std::string& next_page_token) const override;

  IItem::Pointer toItem(const Json::Value&) const;

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

 private:
  std::string openstack_endpoint() const;
  std::string openstack_token() const;

  std::string openstack_endpoint_;
  std::string openstack_token_;
};

}  // namespace cloudstorage

#endif  // HUBIC_H
