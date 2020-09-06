/*****************************************************************************
 * LocalDriveWinRT.h
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
#ifndef LOCALDRIVEWINRT_H
#define LOCALDRIVEWINRT_H

#include "Utility/Utility.h"

#ifdef WINRT

#include "CloudProvider.h"
#include "Utility/Item.h"

namespace cloudstorage {

class LocalDriveWinRT : public CloudProvider {
 public:
  LocalDriveWinRT();

  std::string name() const override;
  std::string endpoint() const override;
  AuthorizeRequest::Pointer authorizeAsync() override;
  bool unpackCredentials(const std::string&) override;
  IItem::Pointer rootDirectory() const override;

  GeneralDataRequest::Pointer getGeneralDataAsync(GeneralDataCallback) override;
  ListDirectoryPageRequest::Pointer listDirectoryPageAsync(
      IItem::Pointer, const std::string&, ListDirectoryPageCallback) override;
  GetItemDataRequest::Pointer getItemDataAsync(const std::string& id,
                                               GetItemDataCallback f) override;
  DownloadFileRequest::Pointer downloadFileAsync(IItem::Pointer,
                                                 IDownloadFileCallback::Pointer,
                                                 Range) override;
  CreateDirectoryRequest::Pointer createDirectoryAsync(
      IItem::Pointer parent, const std::string& name,
      CreateDirectoryCallback) override;
  DeleteItemRequest::Pointer deleteItemAsync(IItem::Pointer,
                                             DeleteItemCallback) override;
  RenameItemRequest::Pointer renameItemAsync(IItem::Pointer item,
                                             const std::string&,
                                             RenameItemCallback) override;
  MoveItemRequest::Pointer moveItemAsync(IItem::Pointer source,
                                         IItem::Pointer destination,
                                         MoveItemCallback) override;
  GetItemUrlRequest::Pointer getItemUrlAsync(IItem::Pointer,
                                             GetItemUrlCallback) override;
  UploadFileRequest::Pointer uploadFileAsync(
      IItem::Pointer, const std::string&,
      IUploadFileCallback::Pointer) override;
};

}  // namespace cloudstorage

#endif

#endif  // LOCALDRIVEWINRT_H
