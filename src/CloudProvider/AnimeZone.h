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
  void authorizeRequest(IHttpRequest& r) const override;
  bool reauthorize(int code,
                   const IHttpRequest::HeaderParameters&) const override;
  std::string name() const override;
  std::string endpoint() const override;
  bool isSuccess(int code,
                 const IHttpRequest::HeaderParameters&) const override;

  GeneralDataRequest::Pointer getGeneralDataAsync(GeneralDataCallback) override;
  GetItemDataRequest::Pointer getItemDataAsync(const std::string& id,
                                               GetItemDataCallback f) override;
  DownloadFileRequest::Pointer downloadFileAsync(
      IItem::Pointer i, IDownloadFileCallback::Pointer cb,
      Range range) override;

  IHttpRequest::Pointer listDirectoryRequest(
      const IItem&, const std::string& page_token,
      std::ostream& input_stream) const override;
  IItem::List listDirectoryResponse(
      const IItem& directory, std::istream& response,
      std::string& next_page_token) const override;

  GetItemUrlRequest::Pointer getItemUrlAsync(IItem::Pointer,
                                             GetItemUrlCallback) override;

 private:
  IItem::List rootDirectoryContent() const;
  IItem::List letterDirectoryContent(const std::string& content,
                                     std::string& next_page_token) const;
  IItem::List animeDirectoryContent(const std::string& anime_name,
                                    const std::string& content) const;
  IItem::List episodeDirectoryContent(const std::string& episode_url,
                                      const std::string& anime_name,
                                      const std::string& episode_no,
                                      const std::string& content) const;

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
