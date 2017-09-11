/*****************************************************************************
 * WebDav.h : WebDav headers
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

#ifndef WEBDAV_H
#define WEBDAV_H

#include <tinyxml2.h>

#include "CloudProvider.h"

namespace cloudstorage {

/**
 * WebDav doesn't use oauth, it does the authorization by sending user
 * and password with every http request. Token in this case is a base64 encoded
 * json with fields username, password, webdav_url.
 * Authorize library url is http://redirect_uri()/login.
 * This webpage is hosted during the Auth::awaitAuthorizationCode and shows text
 * inputs for username and password.
 */
class WebDav : public CloudProvider {
 public:
  WebDav();

  IItem::Pointer rootDirectory() const override;

  void initialize(InitData&&) override;

  std::string name() const override;
  std::string endpoint() const override;
  std::string token() const override;

  AuthorizeRequest::Pointer authorizeAsync() override;
  CreateDirectoryRequest::Pointer createDirectoryAsync(
      IItem::Pointer, const std::string&, CreateDirectoryCallback) override;

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
  IHttpRequest::Pointer deleteItemRequest(
      const IItem&, std::ostream& input_stream) const override;
  IHttpRequest::Pointer moveItemRequest(const IItem&, const IItem&,
                                        std::ostream&) const override;
  IHttpRequest::Pointer renameItemRequest(const IItem&, const std::string& name,
                                          std::ostream&) const override;

  IItem::Pointer getItemDataResponse(std::istream& response) const override;
  std::vector<IItem::Pointer> listDirectoryResponse(
      const IItem&, std::istream&, std::string& next_page_token) const override;
  IItem::Pointer renameItemResponse(const IItem& old_item,
                                    const std::string& name,
                                    std::istream& response) const override;
  IItem::Pointer moveItemResponse(const IItem&, const IItem&,
                                  std::istream&) const override;

  IItem::Pointer toItem(const tinyxml2::XMLNode*) const;

  bool reauthorize(int code,
                   const IHttpRequest::HeaderParameters&) const override;
  void authorizeRequest(IHttpRequest&) const override;

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
  bool unpackCredentials(const std::string& code) override;

  std::string webdav_url_;
  std::string user_;
  std::string password_;
};

}  // namespace cloudstorage

#endif  // WEBDAV_H
