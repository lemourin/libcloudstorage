/*****************************************************************************
 * LocalDrive.h
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
#ifndef LOCALDRIVE_H
#define LOCALDRIVE_H

#ifdef HAVE_BOOST_FILESYSTEM_HPP

#define WITH_LOCALDRIVE

#include "CloudProvider.h"
#include "Request/Request.h"

namespace cloudstorage {

class LocalDrive : public CloudProvider {
 public:
  LocalDrive();

  std::string name() const override;
  std::string endpoint() const override;
  std::string token() const override;

  void initialize(InitData&&) override;
  AuthorizeRequest::Pointer authorizeAsync() override;

  ListDirectoryPageRequest::Pointer listDirectoryPageAsync(
      IItem::Pointer, const std::string&, ListDirectoryPageCallback) override;
  GetItemUrlRequest::Pointer getItemUrlAsync(IItem::Pointer,
                                             GetItemUrlCallback) override;
  DownloadFileRequest::Pointer downloadFileAsync(IItem::Pointer,
                                                 IDownloadFileCallback::Pointer,
                                                 Range) override;
  UploadFileRequest::Pointer uploadFileAsync(
      IItem::Pointer, const std::string&,
      IUploadFileCallback::Pointer) override;
  DeleteItemRequest::Pointer deleteItemAsync(IItem::Pointer,
                                             DeleteItemCallback) override;
  CreateDirectoryRequest::Pointer createDirectoryAsync(
      IItem::Pointer parent, const std::string& name,
      CreateDirectoryCallback) override;
  MoveItemRequest::Pointer moveItemAsync(IItem::Pointer source,
                                         IItem::Pointer destination,
                                         MoveItemCallback) override;
  RenameItemRequest::Pointer renameItemAsync(IItem::Pointer item,
                                             const std::string&,
                                             RenameItemCallback) override;
  GetItemDataRequest::Pointer getItemDataAsync(const std::string& id,
                                               GetItemDataCallback f) override;
  GeneralDataRequest::Pointer getGeneralDataAsync(GeneralDataCallback) override;

  std::string path(IItem::Pointer) const;

 private:
  template <class ReturnValue>
  typename Request<ReturnValue>::Wrapper::Pointer request(
      typename Request<ReturnValue>::Callback,
      typename Request<ReturnValue>::Resolver);

  template <class T>
  void ensureInitialized(typename Request<T>::Pointer r,
                         std::function<void()> on_success);
  bool unpackCredentials(const std::string& code) override;
  std::string path() const;

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

  std::string path_;
};

}  // namespace cloudstorage

#endif  // HAVE_BOOST_FILESYSTEM_HPP
#endif  // LOCALDRIVE_H
