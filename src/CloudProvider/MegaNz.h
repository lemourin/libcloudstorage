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

#ifdef WITH_MEGA

#include "CloudProvider.h"

#include <random>
#include <unordered_set>

namespace mega {
struct Node;
}

namespace cloudstorage {

namespace {
template <class T>
class Listener;
enum class Type;
}  // namespace

class CloudMegaClient;

/**
 * MegaNz doesn't use oauth, Token in this case is a base64 encoded
 * json with fields username, password (hashed). When credentials are required,
 * user is delegated to library's webpage at http://redirect_uri()/login.
 * MegaNz doesn't provide direct urls to their files, because everything they
 * have is encrypted. Here is implemented a mechanism which downloads the file
 * with mega's sdk and forwards it to url
 * file_url/?file=id&state=some_state.
 */
class MegaNz : public CloudProvider {
 public:
  MegaNz();
  ~MegaNz() override;

  void initialize(InitData&&) override;

  std::string name() const override;
  std::string endpoint() const override;
  void destroy() override;

  const std::atomic_bool& authorized() const { return authorized_; }

  ExchangeCodeRequest::Pointer exchangeCodeAsync(const std::string&,
                                                 ExchangeCodeCallback) override;
  AuthorizeRequest::Pointer authorizeAsync() override;
  GetItemDataRequest::Pointer getItemDataAsync(
      const std::string& id, GetItemDataCallback callback) override;
  DownloadFileRequest::Pointer downloadFileAsync(IItem::Pointer,
                                                 IDownloadFileCallback::Pointer,
                                                 Range) override;
  UploadFileRequest::Pointer uploadFileAsync(
      IItem::Pointer, const std::string&,
      IUploadFileCallback::Pointer) override;
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
  GeneralDataRequest::Pointer getGeneralDataAsync(GeneralDataCallback) override;
  DownloadFileRequest::Pointer getThumbnailAsync(
      IItem::Pointer, IDownloadFileCallback::Pointer) override;

  std::function<void(Request<EitherError<void>>::Pointer)> downloadResolver(
      IItem::Pointer item, IDownloadFileCallback*, Range);

  void login(const Request<EitherError<void>>::Pointer& r,
             const std::string& session,
             const AuthorizeRequest::AuthorizeCompleted&);

  std::string passwordHash(const std::string& password) const;

  CloudMegaClient* mega() const { return mega_.get(); }

  IItem::Pointer toItem(mega::Node*);
  mega::Node* node(const std::string& id) const;

  template <class T>
  void ensureAuthorized(const typename Request<T>::Pointer&,
                        std::function<void()> on_success);
  template <class T>
  std::shared_ptr<IRequest<EitherError<T>>> make_request(
      Type, const std::function<void(Listener<T>*, int)>& init,
      const GenericCallback<EitherError<T>>& callback);

 private:
  std::unique_ptr<CloudMegaClient> mega_;
  std::atomic_bool authorized_;
  std::mutex mutex_;
};

}  // namespace cloudstorage

#endif  // WITH_MEGA

#endif  // MEGANZ_H
