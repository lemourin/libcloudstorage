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

  IItem::Pointer rootDirectory() const;

  void initialize(const std::string& token, ICallback::Pointer,
                  ICrypto::Pointer, const Hints& hints);

  std::string name() const;
  std::string token() const;

  AuthorizeRequest::Pointer authorizeAsync();
  CreateDirectoryRequest::Pointer createDirectoryAsync(IItem::Pointer,
                                                       const std::string&,
                                                       CreateDirectoryCallback);

  HttpRequest::Pointer getItemDataRequest(const std::string&,
                                          std::ostream& input_stream) const;
  HttpRequest::Pointer listDirectoryRequest(const IItem&,
                                            const std::string& page_token,
                                            std::ostream& input_stream) const;
  HttpRequest::Pointer uploadFileRequest(const IItem& directory,
                                         const std::string& filename,
                                         std::ostream&, std::ostream&) const;
  HttpRequest::Pointer downloadFileRequest(const IItem&,
                                           std::ostream& input_stream) const;
  HttpRequest::Pointer deleteItemRequest(const IItem&,
                                         std::ostream& input_stream) const;
  HttpRequest::Pointer moveItemRequest(const IItem&, const IItem&,
                                       std::ostream&) const;
  HttpRequest::Pointer renameItemRequest(const IItem&, const std::string& name,
                                         std::ostream&) const;

  IItem::Pointer getItemDataResponse(std::istream& response) const;
  std::vector<IItem::Pointer> listDirectoryResponse(
      std::istream&, std::string& next_page_token) const;

  std::string api_url() const;

  IItem::Pointer toItem(const tinyxml2::XMLNode*) const;

  bool reauthorize(int code) const;
  void authorizeRequest(HttpRequest&) const;

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
  void unpackCreditentials(const std::string& code);

  std::string owncloud_base_url_;
  std::string user_;
  std::string password_;
};

}  // namespace cloudstorage

#endif  // OWNCLOUD_H
