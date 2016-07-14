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

#include <memory>
#include <vector>

#include "IItem.h"

namespace cloudstorage {

template <class ReturnValue>
class IRequest {
 public:
  using Pointer = std::shared_ptr<IRequest>;

  virtual ~IRequest() = default;

  virtual void finish() = 0;
  virtual void cancel() = 0;
  virtual ReturnValue result() = 0;
};

class IListDirectoryCallback {
 public:
  using Pointer = std::shared_ptr<IListDirectoryCallback>;

  virtual ~IListDirectoryCallback() = default;

  virtual void receivedItem(IItem::Pointer item) = 0;
  virtual void done(const std::vector<IItem::Pointer>& result) = 0;
  virtual void error(const std::string& description) = 0;
};

class IDownloadFileCallback {
 public:
  using Pointer = std::shared_ptr<IDownloadFileCallback>;

  virtual ~IDownloadFileCallback() = default;

  virtual void receivedData(const char* data, uint32_t length) = 0;
  virtual void done() = 0;
  virtual void error(const std::string& description) = 0;
  virtual void progress(uint32_t total, uint32_t now) = 0;
};

class IUploadFileCallback {
 public:
  using Pointer = std::shared_ptr<IUploadFileCallback>;

  virtual ~IUploadFileCallback() = default;

  virtual void reset() = 0;
  virtual uint32_t putData(char* data, uint32_t maxlength) = 0;
  virtual void done() = 0;
  virtual void error(const std::string& description) = 0;
  virtual void progress(uint32_t total, uint32_t now) = 0;
};

using GetItemCallback = std::function<void(IItem::Pointer)>;
using GetItemDataCallback = std::function<void(IItem::Pointer)>;

}  // namespace cloudstorage

#endif  // IREQUEST_H
