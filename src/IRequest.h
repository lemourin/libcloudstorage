/*****************************************************************************
 * IRequest : IRequest interface
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

#ifndef IREQUEST_H
#define IREQUEST_H

#include <functional>
#include <memory>
#include <vector>

#include "IItem.h"

namespace cloudstorage {

/**
 * Class representing pending request. When there is no reference to the
 * request, it's immediately cancelled.
 */
class IGenericRequest {
 public:
  virtual ~IGenericRequest() = default;

  /**
   * Blocks until request finishes.
   */
  virtual void finish() = 0;

  /**
   * Cancels request; may involve curl request cancellation which may last very
   * long if curl was compiled without asynchronous name resolver(c-ares).
   */
  virtual void cancel() = 0;
};

template <class ReturnValue>
class IRequest : public IGenericRequest {
 public:
  using Pointer = std::shared_ptr<IRequest>;

  /**
   * Retrieves the result, blocks if it wasn't computed just yet.
   * @return result
   */
  virtual ReturnValue result() = 0;
};

class IListDirectoryCallback {
 public:
  using Pointer = std::shared_ptr<IListDirectoryCallback>;

  virtual ~IListDirectoryCallback() = default;

  /**
   * Called when directory's child was fetched.
   *
   * @param item fetched item
   */
  virtual void receivedItem(IItem::Pointer item) = 0;

  /**
   * Called when the request was successfully finished.
   *
   * @param result contains all retrieved children
   */
  virtual void done(const std::vector<IItem::Pointer>& result) = 0;

  /**
   * Called when error occurred.
   *
   * @param description error description
   */
  virtual void error(const std::string& description) = 0;
};

class IDownloadFileCallback {
 public:
  using Pointer = std::shared_ptr<IDownloadFileCallback>;

  virtual ~IDownloadFileCallback() = default;

  /**
   * Called when received a part of file.
   *
   * @param data buffer
   * @param length length of buffer
   */
  virtual void receivedData(const char* data, uint32_t length) = 0;

  /**
   * Called when the download has finished.
   */
  virtual void done() = 0;

  /**
   * Called when error occurred.
   *
   * @param description error description
   */
  virtual void error(const std::string& description) = 0;

  /**
   * Called when progress has changed.
   *
   * @param total count of bytes to download
   * @param now count of bytes downloaded
   */
  virtual void progress(uint32_t total, uint32_t now) = 0;
};

class IUploadFileCallback {
 public:
  using Pointer = std::shared_ptr<IUploadFileCallback>;

  virtual ~IUploadFileCallback() = default;

  /**
   * Called when upload starts, can be also called when retransmission is
   * required due to network issues.
   */
  virtual void reset() = 0;

  /**
   * Called when the file data should be uploaded.
   *
   * @param data buffer to put data to
   * @param maxlength max count of bytes which can be put to the buffer
   * @return count of bytes put to the buffer
   */
  virtual uint32_t putData(char* data, uint32_t maxlength) = 0;

  /**
   * @return size of currently uploaded file
   */
  virtual uint64_t size() = 0;

  /**
   * Called when the upload is finished sucessfully.
   */
  virtual void done() = 0;

  /**
   * Called when error occurred.
   * @param description
   */
  virtual void error(const std::string& description) = 0;

  /**
   * Called when upload progress changed.
   *
   * @param total count of bytes to upload
   * @param now count of bytes already uploaded
   */
  virtual void progress(uint32_t total, uint32_t now) = 0;
};

using ExchangeCodeCallback = std::function<void(const std::string&)>;
using GetItemCallback = std::function<void(IItem::Pointer)>;
using GetItemDataCallback = std::function<void(IItem::Pointer)>;
using DeleteItemCallback = std::function<void(bool)>;
using CreateDirectoryCallback = std::function<void(IItem::Pointer)>;
using MoveItemCallback = std::function<void(bool)>;
using RenameItemCallback = std::function<void(bool)>;
using ListDirectoryCallback =
    std::function<void(const std::vector<IItem::Pointer>&)>;
using DownloadFileCallback = std::function<void(bool)>;
using UploadFileCallback = std::function<void(bool)>;
using GetThumbnailCallback = std::function<void(bool)>;

}  // namespace cloudstorage

#endif  // IREQUEST_H
