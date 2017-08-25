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
#include <random>
#include <unordered_set>

namespace cloudstorage {

/**
 * MegaNz doesn't use oauth, token in this case is the following:
 * username##hash(password). When credentials are required, user is delegated
 * to library's webpage at http://redirect_uri()/login.
 * MegaNz doesn't provide direct urls to their files, because everything they
 * have is encrypted. Here is implemented a mechanism which downloads the file
 * with mega's sdk and forwards it to url
 * file_url/?file=id&state=some_state.
 */
class MegaNz : public CloudProvider {
 public:
  class HttpServerCallback : public IHttpServer::ICallback {
   public:
    HttpServerCallback(MegaNz*);
    IHttpServer::IResponse::Pointer handle(
        const IHttpServer::IRequest&) override;

   private:
    MegaNz* provider_;
  };

  MegaNz();
  ~MegaNz();

  void initialize(InitData&&) override;

  std::string name() const override;
  std::string endpoint() const override;
  IItem::Pointer rootDirectory() const override;
  Hints hints() const override;

  ExchangeCodeRequest::Pointer exchangeCodeAsync(const std::string&,
                                                 ExchangeCodeCallback) override;
  AuthorizeRequest::Pointer authorizeAsync() override;
  GetItemDataRequest::Pointer getItemDataAsync(
      const std::string& id, GetItemDataCallback callback) override;
  ListDirectoryRequest::Pointer listDirectoryAsync(
      IItem::Pointer, IListDirectoryCallback::Pointer) override;
  DownloadFileRequest::Pointer downloadFileAsync(
      IItem::Pointer, IDownloadFileCallback::Pointer) override;
  UploadFileRequest::Pointer uploadFileAsync(
      IItem::Pointer, const std::string&,
      IUploadFileCallback::Pointer) override;
  DownloadFileRequest::Pointer getThumbnailAsync(
      IItem::Pointer, IDownloadFileCallback::Pointer) override;
  DeleteItemRequest::Pointer deleteItemAsync(IItem::Pointer,
                                             DeleteItemCallback) override;
  CreateDirectoryRequest::Pointer createDirectoryAsync(
      IItem::Pointer, const std::string&, CreateDirectoryCallback) override;
  MoveItemRequest::Pointer moveItemAsync(IItem::Pointer, IItem::Pointer,
                                         MoveItemCallback) override;
  RenameItemRequest::Pointer renameItemAsync(IItem::Pointer item,
                                             const std::string& name,
                                             RenameItemCallback) override;
  ListDirectoryPageRequest::Pointer listDirectoryPageAsync(
      IItem::Pointer, const std::string&, ListDirectoryPageCallback) override;

  std::function<void(Request<EitherError<void>>::Ptr)> downloadResolver(
      IItem::Pointer item, IDownloadFileCallback::Pointer, int64_t start = 0,
      int64_t size = -1);

  void login(Request<EitherError<void>>::Ptr,
             AuthorizeRequest::AuthorizeCompleted);
  std::string passwordHash(const std::string& password) const;

  mega::MegaApi* mega() const { return mega_.get(); }

  IItem::Pointer toItem(mega::MegaNode*);
  std::string randomString(int length);
  std::string temporaryFileName();

  template <class T>
  void ensureAuthorized(typename Request<T>::Ptr,
                        std::function<void(T)> on_error,
                        std::function<void()> on_success);

  IAuth::Token::Pointer authorizationCodeToToken(const std::string& code) const;

  void addStreamRequest(std::shared_ptr<DownloadFileRequest>);
  void removeStreamRequest(std::shared_ptr<DownloadFileRequest>);

  void addRequestListener(std::shared_ptr<IRequest<EitherError<void>>>);
  void removeRequestListener(std::shared_ptr<IRequest<EitherError<void>>>);

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
  std::unique_ptr<mega::MegaApi> mega_;
  std::atomic_bool authorized_;
  std::random_device device_;
  std::default_random_engine engine_;
  std::mutex mutex_;
  IHttpServer::Pointer daemon_;
  std::string temporary_directory_;
  std::string file_url_;
  std::unordered_set<std::shared_ptr<DownloadFileRequest>> stream_requests_;
  std::unordered_set<std::shared_ptr<IRequest<EitherError<void>>>>
      request_listeners_;
  bool deleted_;
};

}  // namespace cloudstorage

#endif  // MEGANZ_H
