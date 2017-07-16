/*****************************************************************************
 * OwnCloud.h : OwnCloud headers
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

#ifndef OWNCLOUD_H
#define OWNCLOUD_H

#include <tinyxml2.h>

#include "CloudProvider.h"

namespace cloudstorage {

/**
 * OwnCloud doesn't use oauth, it does the authorization by sending user
 * and password with every http request. Token in this case is the following:
 * username\@owncloud_base_url##password.
 * Authorize library url is http://localhost:redirect_uri_port()/login.
 * This webpage is hosted during the Auth::awaitAuthorizationCode and shows text
 * inputs for username and password.
 */
class OwnCloud : public CloudProvider {
 public:
  OwnCloud();

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
      std::istream&, std::string& next_page_token) const override;

  std::string api_url() const;

  IItem::Pointer toItem(const tinyxml2::XMLNode*) const;

  bool reauthorize(int code) const override;
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
  bool unpackCredentials(const std::string& code);

  std::string owncloud_base_url_;
  std::string user_;
  std::string password_;
};

}  // namespace cloudstorage

#endif  // OWNCLOUD_H
