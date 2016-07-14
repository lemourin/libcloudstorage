/*****************************************************************************
 * UploadFileRequest.h : UploadFileRequest headers
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

#ifndef UPLOADFILEREQUEST_H
#define UPLOADFILEREQUEST_H

#include "IItem.h"
#include "Request.h"

namespace cloudstorage {

class UploadStreamWrapper : public std::streambuf {
 public:
  static constexpr uint32_t BUFFER_SIZE = 1024;

  UploadStreamWrapper(std::function<uint32_t(char*, uint32_t)> callback);

  int_type underflow();

  char buffer_[BUFFER_SIZE];
  std::function<uint32_t(char*, uint32_t)> callback_;
};

class UploadFileRequest : public Request<void> {
 public:
  using ICallback = IUploadFileCallback;

  UploadFileRequest(std::shared_ptr<CloudProvider>, IItem::Pointer directory,
                    const std::string& filename, ICallback::Pointer);
  ~UploadFileRequest();

 protected:
  void error(int code, const std::string& description);

 private:
  IItem::Pointer directory_;
  std::string filename_;
  UploadStreamWrapper stream_wrapper_;
  ICallback::Pointer callback_;
};

}  // namespace cloudstorage

#endif  // UPLOADFILEREQUEST_H
