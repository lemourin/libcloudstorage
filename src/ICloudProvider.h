/*****************************************************************************
 * ICloudProvider.h : prototypes of ICloudProvider
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

#ifndef ICLOUDPROVIDER_H
#define ICLOUDPROVIDER_H

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "IItem.h"
#include "IRequest.h"

namespace cloudstorage {

class ICloudProvider {
 public:
  using Pointer = std::shared_ptr<ICloudProvider>;
  using Hints = std::unordered_map<std::string, std::string>;

  using ListDirectoryRequest = IRequest<std::vector<IItem::Pointer>>;
  using GetItemRequest = IRequest<IItem::Pointer>;
  using DownloadFileRequest = IRequest<void>;
  using UploadFileRequest = IRequest<void>;
  using GetItemDataRequest = IRequest<IItem::Pointer>;
  using DeleteItemRequest = IRequest<bool>;

  class ICallback {
   public:
    using Pointer = std::unique_ptr<ICallback>;

    enum class Status { WaitForAuthorizationCode, None };

    virtual ~ICallback() = default;
    virtual Status userConsentRequired(const ICloudProvider&) = 0;
    virtual void accepted(const ICloudProvider&) = 0;
    virtual void declined(const ICloudProvider&) = 0;
    virtual void error(const ICloudProvider&,
                       const std::string& description) = 0;
  };

  virtual ~ICloudProvider() = default;

  virtual void initialize(const std::string& token, ICallback::Pointer,
                          const Hints& hints = Hints()) = 0;
  virtual std::string token() const = 0;
  virtual Hints hints() const = 0;
  virtual std::string name() const = 0;
  virtual std::string authorizeLibraryUrl() const = 0;
  virtual IItem::Pointer rootDirectory() const = 0;

  virtual ListDirectoryRequest::Pointer listDirectoryAsync(
      IItem::Pointer, IListDirectoryCallback::Pointer) = 0;
  virtual GetItemRequest::Pointer getItemAsync(const std::string& absolute_path,
                                               GetItemCallback) = 0;
  virtual DownloadFileRequest::Pointer downloadFileAsync(
      IItem::Pointer, IDownloadFileCallback::Pointer) = 0;
  virtual UploadFileRequest::Pointer uploadFileAsync(
      IItem::Pointer, const std::string& filename,
      IUploadFileCallback::Pointer) = 0;
  virtual GetItemDataRequest::Pointer getItemDataAsync(const std::string& id,
                                                       GetItemDataCallback) = 0;
  virtual DownloadFileRequest::Pointer getThumbnailAsync(
      IItem::Pointer item, IDownloadFileCallback::Pointer) = 0;
  virtual DeleteItemRequest::Pointer deleteItemAsync(IItem::Pointer,
                                                     DeleteItemCallback) = 0;
};

}  // namespace cloudstorage

#endif  // ICLOUDPROVIDER_H
