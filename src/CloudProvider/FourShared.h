/*****************************************************************************
 * FourShared.h
 *
 *****************************************************************************
 * Copyright (C) 2018 VideoLAN
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
#ifndef FOURSHARED_H
#define FOURSHARED_H

#include "CloudProvider.h"

namespace cloudstorage {

class FourShared : public CloudProvider {
 public:
  FourShared();

  void initialize(InitData&&) override;
  std::string name() const override;
  std::string endpoint() const override;
  IItem::Pointer rootDirectory() const override;
  void authorizeRequest(IHttpRequest& request) const override;
  bool reauthorize(int code,
                   const IHttpRequest::HeaderParameters&) const override;
  Hints hints() const override;
  std::string token() const override;

  AuthorizeRequest::Pointer authorizeAsync() override;
  bool unpackCredentials(const std::string& code) override;

  std::string username() const;
  std::string password() const;

  ExchangeCodeRequest::Pointer exchangeCodeAsync(const std::string&,
                                                 ExchangeCodeCallback) override;
  GetItemUrlRequest::Pointer getItemUrlAsync(
      IItem::Pointer item, GetItemUrlCallback callback) override;
  DownloadFileRequest::Pointer downloadFileAsync(IItem::Pointer,
                                                 IDownloadFileCallback::Pointer,
                                                 Range) override;

  IHttpRequest::Pointer getItemDataRequest(
      const std::string&, std::ostream& input_stream) const override;
  IItem::Pointer getItemDataResponse(std::istream& response) const override;

  IHttpRequest::Pointer listDirectoryRequest(
      const IItem&, const std::string& page_token,
      std::ostream& input_stream) const override;
  IItem::List listDirectoryResponse(
      const IItem&, std::istream&, std::string& next_page_token) const override;

  IHttpRequest::Pointer uploadFileRequest(
      const IItem& directory, const std::string& filename,
      std::ostream& prefix_stream, std::ostream& suffix_stream) const override;
  IHttpRequest::Pointer deleteItemRequest(
      const IItem&, std::ostream& input_stream) const override;
  IHttpRequest::Pointer createDirectoryRequest(const IItem&,
                                               const std::string& name,
                                               std::ostream&) const override;
  IHttpRequest::Pointer moveItemRequest(const IItem&, const IItem&,
                                        std::ostream&) const override;
  IHttpRequest::Pointer renameItemRequest(const IItem&, const std::string& name,
                                          std::ostream&) const override;

  IHttpRequest::Pointer getGeneralDataRequest(std::ostream&) const override;
  GeneralData getGeneralDataResponse(std::istream& response) const override;

 private:
  IItem::Pointer toItem(const Json::Value&) const;
  std::string root_id() const;

  class Auth : public cloudstorage::Auth {
   public:
    void initialize(IHttp*, IHttpServerFactory*) override;
    std::string authorizeLibraryUrl() const override;

    IHttpRequest::Pointer exchangeAuthorizationCodeRequest(
        std::ostream& input_data) const override;
    IHttpRequest::Pointer refreshTokenRequest(
        std::ostream& input_data) const override;

    Token::Pointer exchangeAuthorizationCodeResponse(
        std::istream&) const override;
    Token::Pointer refreshTokenResponse(std::istream&) const override;
  };

  std::string root_id_;
  std::string username_;
  std::string password_;
  std::string oauth_token_;
  std::string oauth_token_secret_;
};

}  // namespace cloudstorage

#endif  // FOURSHARED_H
