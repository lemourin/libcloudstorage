/*****************************************************************************
 * MegaNz.h : Mega headers
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
#include <microhttpd.h>
#include <random>

namespace cloudstorage {

/**
 * MegaNz doesn't use oauth, token in this case is the following:
 * username##hash(password). When creditentials are required, user is delegated
 * to library's webpage at http://localhost:redirect_url()/login.
 * MegaNz doesn't provide direct urls to their files, because everything they
 * have is encrypted. Here is implemented a mechanism which downloads the file
 * with mega's sdk and forwards it to url http://localhost:daemon_url/?file=id.
 * This url isn't seekable which may be problematic for some media players.
 */
class MegaNz : public CloudProvider {
 public:
  MegaNz();
  ~MegaNz();

  void initialize(const std::string& token, ICallback::Pointer,
                  const Hints& hints);

  std::string name() const;
  IItem::Pointer rootDirectory() const;
  Hints hints() const;

  AuthorizeRequest::Pointer authorizeAsync();
  GetItemDataRequest::Pointer getItemDataAsync(const std::string& id,
                                               GetItemDataCallback callback);
  ListDirectoryRequest::Pointer listDirectoryAsync(
      IItem::Pointer, IListDirectoryCallback::Pointer);
  DownloadFileRequest::Pointer downloadFileAsync(
      IItem::Pointer, IDownloadFileCallback::Pointer);
  UploadFileRequest::Pointer uploadFileAsync(IItem::Pointer, const std::string&,
                                             IUploadFileCallback::Pointer);
  DownloadFileRequest::Pointer getThumbnailAsync(
      IItem::Pointer, IDownloadFileCallback::Pointer);
  DeleteItemRequest::Pointer deleteItemAsync(IItem::Pointer,
                                             DeleteItemCallback);
  CreateDirectoryRequest::Pointer createDirectoryAsync(IItem::Pointer,
                                                       const std::string&,
                                                       CreateDirectoryCallback);
  MoveItemRequest::Pointer moveItemAsync(IItem::Pointer, IItem::Pointer,
                                         MoveItemCallback);
  RenameItemRequest::Pointer renameItemAsync(IItem::Pointer item,
                                             const std::string& name,
                                             RenameItemCallback);

  std::pair<std::string, std::string> creditentialsFromString(
      const std::string&) const;

  bool login(Request<bool>* r);
  std::string passwordHash(const std::string& password);

  mega::MegaApi* mega() const { return mega_.get(); }

  IItem::Pointer toItem(mega::MegaNode*);
  std::string randomString(int length);

  template <class T>
  bool ensureAuthorized(Request<T>*);

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

 private:
  std::unique_ptr<mega::MegaApi> mega_;
  std::atomic_bool authorized_;
  std::random_device device_;
  std::default_random_engine engine_;
  mutable std::mutex mutex_;
  uint16_t daemon_port_;
  MHD_Daemon* daemon_;
};

}  // namespace cloudstorage

#endif  // MEGANZ_H