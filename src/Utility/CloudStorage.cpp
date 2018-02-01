/*****************************************************************************
 * CloudStorage.cpp : implementation of CloudStorage
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

#include "CloudStorage.h"

#include "CloudProvider/AmazonS3.h"
#include "CloudProvider/Box.h"
#include "CloudProvider/Dropbox.h"
#include "CloudProvider/GoogleDrive.h"
#include "CloudProvider/GooglePhotos.h"
#include "CloudProvider/HubiC.h"
#include "CloudProvider/LocalDrive.h"
#include "CloudProvider/MegaNz.h"
#include "CloudProvider/OneDrive.h"
#include "CloudProvider/PCloud.h"
#include "CloudProvider/WebDav.h"
#include "CloudProvider/YandexDisk.h"
#include "CloudProvider/YouTube.h"

#include "Utility/Utility.h"

namespace cloudstorage {

namespace {

class CloudProviderWrapper : public ICloudProvider {
 public:
  CloudProviderWrapper(std::shared_ptr<CloudProvider> p) : p_(p) {}

  ~CloudProviderWrapper() { p_->destroy(); }

  std::string token() const override { return p_->token(); }

  Hints hints() const override { return p_->hints(); }

  std::string name() const override { return p_->name(); }

  std::string endpoint() const override { return p_->endpoint(); }

  OperationSet supportedOperations() const override {
    return p_->supportedOperations();
  }

  std::string authorizeLibraryUrl() const override {
    return p_->authorizeLibraryUrl();
  }

  IItem::Pointer rootDirectory() const override { return p_->rootDirectory(); }

  ExchangeCodeRequest::Pointer exchangeCodeAsync(
      const std::string& code, ExchangeCodeCallback cb) override {
    return p_->exchangeCodeAsync(code, cb);
  }

  ListDirectoryRequest::Pointer listDirectoryAsync(
      IItem::Pointer directory, IListDirectoryCallback::Pointer cb) override {
    return p_->listDirectoryAsync(directory, cb);
  }

  GetItemUrlRequest::Pointer getItemUrlAsync(IItem::Pointer item,
                                             GetItemUrlCallback cb) override {
    return p_->getItemUrlAsync(item, cb);
  }

  GetItemRequest::Pointer getItemAsync(const std::string& absolute_path,
                                       GetItemCallback callback) override {
    return p_->getItemAsync(absolute_path, callback);
  }

  DownloadFileRequest::Pointer downloadFileAsync(
      IItem::Pointer item, IDownloadFileCallback::Pointer cb,
      Range range) override {
    return p_->downloadFileAsync(item, cb, range);
  }

  UploadFileRequest::Pointer uploadFileAsync(
      IItem::Pointer parent, const std::string& filename,
      IUploadFileCallback::Pointer cb) override {
    return p_->uploadFileAsync(parent, filename, cb);
  }

  GetItemDataRequest::Pointer getItemDataAsync(
      const std::string& id, GetItemDataCallback callback) override {
    return p_->getItemDataAsync(id, callback);
  }

  DownloadFileRequest::Pointer getThumbnailAsync(
      IItem::Pointer item, IDownloadFileCallback::Pointer cb) override {
    return p_->getThumbnailAsync(item, cb);
  }

  DeleteItemRequest::Pointer deleteItemAsync(
      IItem::Pointer item, DeleteItemCallback callback) override {
    return p_->deleteItemAsync(item, callback);
  }

  CreateDirectoryRequest::Pointer createDirectoryAsync(
      IItem::Pointer parent, const std::string& name,
      CreateDirectoryCallback callback) override {
    return p_->createDirectoryAsync(parent, name, callback);
  }

  MoveItemRequest::Pointer moveItemAsync(IItem::Pointer source,
                                         IItem::Pointer destination,
                                         MoveItemCallback callback) override {
    return p_->moveItemAsync(source, destination, callback);
  }

  RenameItemRequest::Pointer renameItemAsync(
      IItem::Pointer item, const std::string& name,
      RenameItemCallback callback) override {
    return p_->renameItemAsync(item, name, callback);
  }

  ListDirectoryPageRequest::Pointer listDirectoryPageAsync(
      IItem::Pointer directory, const std::string& token,
      ListDirectoryPageCallback cb) override {
    return p_->listDirectoryPageAsync(directory, token, cb);
  }

  ListDirectoryRequest::Pointer listDirectoryAsync(
      IItem::Pointer item, ListDirectoryCallback callback) override {
    return p_->listDirectoryAsync(item, callback);
  }

  DownloadFileRequest::Pointer downloadFileAsync(
      IItem::Pointer item, const std::string& filename,
      DownloadFileCallback callback) override {
    return p_->downloadFileAsync(item, filename, callback);
  }

  DownloadFileRequest::Pointer getThumbnailAsync(
      IItem::Pointer item, const std::string& filename,
      GetThumbnailCallback callback) override {
    return p_->getThumbnailAsync(item, filename, callback);
  }

  UploadFileRequest::Pointer uploadFileAsync(
      IItem::Pointer parent, const std::string& path,
      const std::string& filename, UploadFileCallback callback) override {
    return p_->uploadFileAsync(parent, path, filename, callback);
  }

  GeneralDataRequest::Pointer getGeneralDataAsync(
      GeneralDataCallback callback) override {
    return p_->getGeneralDataAsync(callback);
  }

 private:
  std::shared_ptr<CloudProvider> p_;
};

}  // namespace

CloudStorage::CloudStorage() {
  add<GoogleDrive>();
  add<OneDrive>();
  add<Dropbox>();
  add<Box>();
  add<YouTube>();
  add<YandexDisk>();
  add<WebDav>();
  add<AmazonS3>();
  add<PCloud>();
  add<HubiC>();
  add<GooglePhotos>();
#ifdef WITH_LOCALDRIVE
  add<LocalDrive>();
#endif
#ifdef WITH_MEGA
  add<MegaNz>();
#endif
}

std::vector<std::string> CloudStorage::providers() const {
  std::vector<std::string> result;
  for (auto r : providers_) result.push_back(r.first);
  return result;
}

ICloudProvider::Pointer CloudStorage::provider(
    const std::string& name, ICloudProvider::InitData&& init_data) const {
  auto it = providers_.find(name);
  if (it == std::end(providers_)) return nullptr;
  auto ret = it->second();
  ret->initialize(std::move(init_data));
  return util::make_unique<CloudProviderWrapper>(ret);
}

ICloudStorage::Pointer ICloudStorage::create() {
  return util::make_unique<CloudStorage>();
}

void CloudStorage::add(CloudProviderFactory f) { providers_[f()->name()] = f; }

}  // namespace cloudstorage
