/*****************************************************************************
 * UploadFileRequest.cpp : UploadFileRequest implementation
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

#include "UploadFileRequest.h"

#include "CloudProvider/CloudProvider.h"

using namespace std::placeholders;

namespace cloudstorage {

UploadFileRequest::UploadFileRequest(
    std::shared_ptr<CloudProvider> p, IItem::Pointer directory,
    const std::string& filename, UploadFileRequest::ICallback::Pointer callback)
    : Request(p),
      directory_(std::move(directory)),
      filename_(filename),
      stream_wrapper_(std::bind(&ICallback::putData, callback.get(), _1, _2),
                      callback->size()),
      callback_(callback) {
  set_resolver([this](Request*) -> EitherError<void> {
    if (directory_->type() != IItem::FileType::Directory) {
      Error e{403, "can't upload into non-directory"};
      callback_->done(e);
      return e;
    }
    std::stringstream response_stream;
    Error error;
    int code = sendRequest(
        [this](std::ostream& input) {
          callback_->reset();
          stream_wrapper_.reset();
          input.rdbuf(&stream_wrapper_);
          return provider()->uploadFileRequest(*directory_, filename_,
                                               stream_wrapper_.prefix_,
                                               stream_wrapper_.suffix_);
        },
        response_stream, &error,
        std::bind(&ICallback::progress, callback_.get(), _1, _2));
    if (IHttpRequest::isSuccess(code)) {
      callback_->done(nullptr);
      return nullptr;
    } else {
      callback_->done(error);
      return error;
    }
  });
}

UploadFileRequest::~UploadFileRequest() { cancel(); }

UploadStreamWrapper::UploadStreamWrapper(
    std::function<uint32_t(char*, uint32_t)> callback, uint64_t size)
    : callback_(std::move(callback)), size_(size), read_(), position_() {}

void UploadStreamWrapper::reset() {
  prefix_ = std::stringstream();
  suffix_ = std::stringstream();
  read_ = 0;
}

UploadStreamWrapper::pos_type UploadStreamWrapper::seekoff(
    off_type off, std::ios_base::seekdir way, std::ios_base::openmode) {
  if (off != 0) return pos_type(off_type(-1));
  if (way == std::ios_base::beg)
    return position_ = 0;
  else if (way == std::ios_base::end)
    return position_ = prefix_.str().size() + size_ + suffix_.str().size();
  else
    return position_;
}

std::streambuf::int_type UploadStreamWrapper::underflow() {
  pos_type read_data = 0;
  if (prefix_) {
    prefix_.read(buffer_ + read_data, BUFFER_SIZE - read_data);
    read_data += prefix_.gcount();
  }
  if (read_ < size_ && !prefix_) {
    uint32_t size = callback_(buffer_ + read_data, BUFFER_SIZE - read_data);
    read_data += size;
    read_ += size;
  }
  if (read_ == size_ && !prefix_) {
    suffix_.read(buffer_ + read_data, BUFFER_SIZE - read_data);
    read_data += suffix_.gcount();
  }
  if (gptr() == egptr()) setg(buffer_, buffer_, buffer_ + read_data);
  return gptr() == egptr() ? std::char_traits<char>::eof()
                           : std::char_traits<char>::to_int_type(*gptr());
}

}  // namespace cloudstorage
