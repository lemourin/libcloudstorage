/*****************************************************************************
 * OneDrive.h : prototypes for OneDrive
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

#ifndef ONEDRIVE_H
#define ONEDRIVE_H

#include <json/forwards.h>

#include "CloudProvider.h"
#include "Utility/Auth.h"

namespace cloudstorage {

class OneDrive : public CloudProvider {
 public:
  OneDrive();

  std::string name() const override;
  std::string endpoint() const override;

  IItem::Pointer toItem(const Json::Value&) const;

 protected:
  void initialize(InitData&&) override;
  bool reauthorize(int code,
                   const IHttpRequest::HeaderParameters& h) const override;
  Hints hints() const override;

  AuthorizeRequest::Pointer authorizeAsync() override;
  UploadFileRequest::Pointer uploadFileAsync(
      IItem::Pointer, const std::string& filename,
      IUploadFileCallback::Pointer) override;
  GeneralDataRequest::Pointer getGeneralDataAsync(GeneralDataCallback) override;

  IHttpRequest::Pointer getItemDataRequest(
      const std::string&, std::ostream& input_stream) const override;
  IHttpRequest::Pointer listDirectoryRequest(
      const IItem&, const std::string& page_token,
      std::ostream& input_stream) const override;
  IHttpRequest::Pointer downloadFileRequest(
      const IItem&, std::ostream& input_stream) const override;
  IHttpRequest::Pointer deleteItemRequest(
      const IItem&, std::ostream& input_stream) const override;
  IHttpRequest::Pointer createDirectoryRequest(const IItem&,
                                               const std::string& name,
                                               std::ostream&) const override;
  IHttpRequest::Pointer moveItemRequest(const IItem&, const IItem&,
                                        std::ostream&) const override;
  IHttpRequest::Pointer renameItemRequest(const IItem&, const std::string& name,
                                          std::ostream&) const override;

  IItem::List listDirectoryResponse(const IItem&, std::istream&,
                                    std::string&) const override;
  IItem::Pointer getItemDataResponse(std::istream& response) const override;

 private:
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

    bool requiresCodeExchange() const override;
  };

  std::string endpoint_;
};

}  // namespace cloudstorage

#endif  // ONEDRIVE_H
