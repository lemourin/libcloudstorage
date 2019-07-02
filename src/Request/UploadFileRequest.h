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
  using Pointer = std::shared_ptr<UploadStreamWrapper>;

  static constexpr uint32_t BUFFER_SIZE = 1024;

  UploadStreamWrapper(
      std::function<uint32_t(char*, uint32_t, uint64_t)> callback,
      uint64_t size);
  void reset();

  pos_type seekoff(off_type, std::ios_base::seekdir,
                   std::ios_base::openmode) override;
  int_type underflow() override;

  char buffer_[BUFFER_SIZE];
  std::function<uint32_t(char*, uint32_t, uint64_t)> callback_;
  std::stringstream prefix_;
  std::stringstream suffix_;
  uint64_t size_;
  uint64_t read_;
  pos_type position_;
};

class UploadFileRequest : public Request<EitherError<IItem>> {
 public:
  using ICallback = IUploadFileCallback;

  UploadFileRequest(std::shared_ptr<CloudProvider>,
                    const IItem::Pointer& directory,
                    const std::string& filename, const ICallback::Pointer&);

  static void resolve(const Request::Pointer&,
                      const UploadStreamWrapper::Pointer&,
                      const IItem::Pointer& directory,
                      const std::string& filename, const ICallback::Pointer&);
};

}  // namespace cloudstorage

#endif  // UPLOADFILEREQUEST_H
