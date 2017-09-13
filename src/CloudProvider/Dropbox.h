/*****************************************************************************
 * Dropbox.h : prototypes for Dropbox
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

#ifndef DROPBOX_H
#define DROPBOX_H

#include <json/forwards.h>

#include "CloudProvider.h"

namespace cloudstorage {

class Dropbox : public CloudProvider {
 public:
  Dropbox();

  std::string name() const override;
  std::string endpoint() const override;
  IItem::Pointer rootDirectory() const override;
  bool reauthorize(int code,
                   const IHttpRequest::HeaderParameters&) const override;

  IHttpRequest::Pointer getItemUrlRequest(
      const IItem&, std::ostream& input_stream) const override;
  IHttpRequest::Pointer getItemDataRequest(
      const std::string&, std::ostream& input_stream) const override;
  IHttpRequest::Pointer listDirectoryRequest(
      const IItem&, const std::string& page_token,
      std::ostream& input_stream) const override;
  IHttpRequest::Pointer uploadFileRequest(const IItem& directory,
                                          const std::string& filename,
                                          std::ostream&,
                                          std::ostream&) const override;
  IHttpRequest::Pointer downloadFileRequest(
      const IItem&, std::ostream& input_stream) const override;
  IHttpRequest::Pointer getThumbnailRequest(
      const IItem&, std::ostream& input_stream) const override;
  IHttpRequest::Pointer deleteItemRequest(
      const IItem&, std::ostream& input_stream) const override;
  IHttpRequest::Pointer createDirectoryRequest(const IItem&,
                                               const std::string& name,
                                               std::ostream&) const override;
  IHttpRequest::Pointer moveItemRequest(const IItem&, const IItem&,
                                        std::ostream&) const override;
  IHttpRequest::Pointer renameItemRequest(const IItem& item,
                                          const std::string& name,
                                          std::ostream&) const override;

  std::vector<IItem::Pointer> listDirectoryResponse(
      const IItem&, std::istream&, std::string& next_page_token) const override;
  std::string getItemUrlResponse(const IItem& item,
                                 const IHttpRequest::HeaderParameters&,
                                 std::istream& response) const override;
  IItem::Pointer getItemDataResponse(std::istream& response) const override;
  IItem::Pointer createDirectoryResponse(const IItem& parent,
                                         const std::string& name,
                                         std::istream& response) const override;
  IItem::Pointer uploadFileResponse(const IItem& parent,
                                    const std::string& filename, uint64_t,
                                    std::istream& response) const override;

  void authorizeRequest(IHttpRequest&) const override;

 private:
  static IItem::Pointer toItem(const Json::Value&);

  class Auth : public cloudstorage::Auth {
   public:
    Auth();

    std::string authorizeLibraryUrl() const override;
    Token::Pointer fromTokenString(const std::string&) const override;

    IHttpRequest::Pointer exchangeAuthorizationCodeRequest(
        std::ostream& input_data) const override;
    IHttpRequest::Pointer refreshTokenRequest(
        std::ostream& input_data) const override;

    Token::Pointer exchangeAuthorizationCodeResponse(
        std::istream&) const override;
    Token::Pointer refreshTokenResponse(std::istream&) const override;
  };
};

}  // namespace cloudstorage

#endif  // DROPBOX_H
