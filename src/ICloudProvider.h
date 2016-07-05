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
#include <vector>

#include "IItem.h"
#include "Request/AuthorizeRequest.h"
#include "Request/DownloadFileRequest.h"
#include "Request/GetItemDataRequest.h"
#include "Request/GetItemRequest.h"
#include "Request/ListDirectoryRequest.h"
#include "Request/UploadFileRequest.h"

namespace cloudstorage {

class ICloudProvider {
 public:
  using Pointer = std::shared_ptr<ICloudProvider>;

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

  virtual AuthorizeRequest::Pointer initialize(const std::string& token,
                                               ICallback::Pointer) = 0;

  virtual std::string token() const = 0;
  virtual std::string name() const = 0;
  virtual std::string authorizeLibraryUrl() const = 0;
  virtual IItem::Pointer rootDirectory() const = 0;

  virtual ListDirectoryRequest::Pointer listDirectoryAsync(
      IItem::Pointer, ListDirectoryRequest::ICallback::Pointer) = 0;
  virtual GetItemRequest::Pointer getItemAsync(
      const std::string& absolute_path,
      std::function<void(IItem::Pointer)>) = 0;
  virtual DownloadFileRequest::Pointer downloadFileAsync(
      IItem::Pointer, DownloadFileRequest::ICallback::Pointer) = 0;
  virtual UploadFileRequest::Pointer uploadFileAsync(
      IItem::Pointer, const std::string& filename,
      UploadFileRequest::ICallback::Pointer) = 0;
  virtual GetItemDataRequest::Pointer getItemDataAsync(
      IItem::Pointer item, std::function<void(IItem::Pointer)>) = 0;
  virtual DownloadFileRequest::Pointer getThumbnailAsync(
      IItem::Pointer item, DownloadFileRequest::ICallback::Pointer) = 0;
};

}  // namespace cloudstorage

#endif  // ICLOUDPROVIDER_H
