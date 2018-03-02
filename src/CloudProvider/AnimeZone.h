/*****************************************************************************
 * AnimeZone.h
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
#ifndef ANIMEZONE_H
#define ANIMEZONE_H

#include "CloudProvider.h"

namespace cloudstorage {

class AnimeZone : public CloudProvider {
 public:
  AnimeZone();

  AuthorizeRequest::Pointer authorizeAsync() override;
  std::string name() const override;
  std::string endpoint() const override;

  GeneralDataRequest::Pointer getGeneralDataAsync(GeneralDataCallback) override;
  GetItemDataRequest::Pointer getItemDataAsync(const std::string& id,
                                               GetItemDataCallback f) override;
  DownloadFileRequest::Pointer downloadFileAsync(
      IItem::Pointer i, IDownloadFileCallback::Pointer cb,
      Range range) override;

  ListDirectoryRequest::Pointer listDirectoryAsync(
      IItem::Pointer, IListDirectoryCallback::Pointer) override;
  ListDirectoryPageRequest::Pointer listDirectoryPageAsync(
      IItem::Pointer, const std::string&, ListDirectoryPageCallback) override;

  IHttpRequest::Pointer listDirectoryRequest(
      const IItem&, const std::string& page_token,
      std::ostream& input_stream) const override;
  IItem::List listDirectoryResponse(const IItem& directory,
                                    std::istream& response,
                                    const IHttpRequest::HeaderParameters&,
                                    std::string& next_page_token) const;

  GetItemUrlRequest::Pointer getItemUrlAsync(IItem::Pointer,
                                             GetItemUrlCallback) override;

 private:
  class Auth : public cloudstorage::Auth {
   public:
    std::string authorizeLibraryUrl() const override;
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

#endif  // ANIMEZONE_H
