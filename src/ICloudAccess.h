/*****************************************************************************
 * ICloudAccess.h
 *
 *****************************************************************************
 * Copyright (C) 2019 VideoLAN
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
#ifndef ICLOUDACCESS_H
#define ICLOUDACCESS_H

#include <memory>

#include "IItem.h"
#include "IRequest.h"
#include "Utility/Promise.h"

namespace cloudstorage {

using ::util::Promise;

class ICloudUploadCallback {
 public:
  virtual ~ICloudUploadCallback() = default;

  virtual uint32_t putData(char* data, uint32_t maxlength, uint64_t offset) = 0;
  virtual uint64_t size() = 0;
  virtual void progress(uint64_t total, uint64_t now) = 0;
};

class ICloudDownloadCallback {
 public:
  virtual ~ICloudDownloadCallback() = default;
  virtual void receivedData(const char* data, uint32_t length) = 0;
  virtual void progress(uint64_t total, uint64_t now) = 0;
};

class CLOUDSTORAGE_API ICloudAccess {
 public:
  using Pointer = std::unique_ptr<ICloudAccess>;
  using ProgressCallback = std::function<void(uint64_t total, uint64_t now)>;

  virtual ~ICloudAccess() = default;

  virtual std::string name() const = 0;
  virtual IItem::Pointer root() const = 0;
  virtual std::string token() const = 0;

  virtual Promise<Token> exchangeCode(const std::string& code) = 0;
  virtual Promise<GeneralData> generalData() = 0;
  virtual Promise<IItem::List> listDirectory(IItem::Pointer item) = 0;
  virtual Promise<IItem::Pointer> getItem(const std::string& path) = 0;
  virtual Promise<std::string> getDaemonUrl(IItem::Pointer item) = 0;
  virtual Promise<std::string> getFileUrl(IItem::Pointer item) = 0;
  virtual Promise<IItem::Pointer> getItemData(const std::string& id) = 0;
  virtual Promise<> deleteItem(IItem::Pointer item) = 0;
  virtual Promise<IItem::Pointer> createDirectory(
      IItem::Pointer parent, const std::string& filename) = 0;
  virtual Promise<IItem::Pointer> moveItem(IItem::Pointer item,
                                           IItem::Pointer new_parent) = 0;
  virtual Promise<IItem::Pointer> renameItem(IItem::Pointer item,
                                             const std::string& new_name) = 0;
  virtual Promise<PageData> listDirectoryPage(IItem::Pointer item,
                                              const std::string& token) = 0;
  virtual Promise<IItem::Pointer> uploadFile(
      IItem::Pointer parent, const std::string& filename,
      std::unique_ptr<ICloudUploadCallback>&&) = 0;
  virtual Promise<> downloadFile(
      IItem::Pointer file, Range range,
      const std::shared_ptr<ICloudDownloadCallback>&) = 0;
  virtual Promise<> downloadThumbnail(
      IItem::Pointer file, const std::shared_ptr<ICloudDownloadCallback>&) = 0;
  virtual Promise<> generateThumbnail(
      IItem::Pointer file, const std::shared_ptr<ICloudDownloadCallback>&) = 0;
  virtual Promise<IItem::Pointer> copyItem(
      IItem::Pointer source_item,
      const std::shared_ptr<ICloudAccess>& target_provider,
      IItem::Pointer target_parent, const std::string& target_filename,
      std::unique_ptr<std::iostream>&& buffer,
      const ProgressCallback& progress = nullptr) = 0;

  static std::unique_ptr<ICloudUploadCallback> streamUploader(
      const std::shared_ptr<std::istream>& stream,
      const ProgressCallback& progress = nullptr);
  static std::unique_ptr<ICloudDownloadCallback> streamDownloader(
      const std::shared_ptr<std::ostream>& stream,
      const ProgressCallback& progress = nullptr);
};
}  // namespace cloudstorage

#endif  // ICLOUDACCESS_H
