/*****************************************************************************
 * Mega.h : Mega headers
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

#ifndef MEGANZ_H
#define MEGANZ_H

#include "CloudProvider.h"

#include <megaapi.h>

namespace cloudstorage {

class MegaNz : public CloudProvider {
 public:
  MegaNz();

  std::string name() const;
  IItem::Pointer rootDirectory() const;

  AuthorizeRequest::Pointer authorizeAsync();
  GetItemDataRequest::Pointer getItemDataAsync(const std::string& id,
                                               GetItemDataCallback callback);
  ListDirectoryRequest::Pointer listDirectoryAsync(
      IItem::Pointer, IListDirectoryCallback::Pointer);
  DownloadFileRequest::Pointer downloadFileAsync(
      IItem::Pointer, IDownloadFileCallback::Pointer);
  UploadFileRequest::Pointer uploadFileAsync(IItem::Pointer, const std::string&,
                                             IUploadFileCallback::Pointer);

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

  class Authorize : public cloudstorage::AuthorizeRequest {
   public:
    using AuthorizeRequest::AuthorizeRequest;
    ~Authorize();
  };

  IItem::Pointer toItem(mega::MegaNode*);

 private:
  mega::MegaApi mega_;
  std::atomic_bool authorized_;
};

}  // namespace cloudstorage

#endif  // MEGANZ_H
