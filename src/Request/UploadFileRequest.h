/*****************************************************************************
 * UploadFileRequest.h : UploadFileRequest headers
 *
 *****************************************************************************
 * Copyright (C) 2016-2016 VideoLAN
 *
 * Authors: Paweł Wegner <pawel.wegner95@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef UPLOADFILEREQUEST_H
#define UPLOADFILEREQUEST_H

#include "Request.h"

namespace cloudstorage {

class UploadFileRequest : public Request {
 public:
  using Pointer = std::unique_ptr<UploadFileRequest>;

  class ICallback {
   public:
    using Pointer = std::unique_ptr<ICallback>;
    virtual ~ICallback() = default;

    virtual void reset() = 0;
    virtual uint32_t putData(char* data, uint32_t maxlength) = 0;
    virtual void done() = 0;
    virtual void error(const std::string& description) = 0;
    virtual void progress(uint32_t total, uint32_t now) = 0;
  };

  UploadFileRequest(std::shared_ptr<CloudProvider>, IItem::Pointer directory,
                    const std::string& filename, ICallback::Pointer);
  ~UploadFileRequest();

  void finish();

 private:
  class UploadStreamWrapper : public std::streambuf {
   public:
    static constexpr uint32_t BUFFER_SIZE = 1024;

    UploadStreamWrapper(UploadFileRequest::ICallback::Pointer callback);

    int_type underflow();

    char buffer_[BUFFER_SIZE];
    UploadFileRequest::ICallback::Pointer callback_;
  };

  int upload(std::ostream& error_stream);

  std::future<void> function_;
  IItem::Pointer directory_;
  std::string filename_;
  UploadStreamWrapper stream_wrapper_;
};

}  // namespace cloudstorage

#endif  // UPLOADFILEREQUEST_H
