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

UploadFileRequest::UploadFileRequest(std::shared_ptr<CloudProvider> p,
                                     IItem::Pointer directory,
                                     const std::string& filename,
                                     UploadFileRequest::ICallback::Pointer cb)
    : Request(
          p, [=](EitherError<IItem> e) { cb->done(e); },
          std::bind(&UploadFileRequest::resolve, _1,
                    std::make_shared<UploadStreamWrapper>(
                        std::bind(&ICallback::putData, cb.get(), _1, _2, _3),
                        cb->size()),
                    directory, filename, cb)) {}

void UploadFileRequest::resolve(Request::Pointer r,
                                UploadStreamWrapper::Pointer stream_wrapper,
                                IItem::Pointer directory, std::string filename,
                                ICallback::Pointer callback) {
  r->send(
      [=](util::Output) {
        stream_wrapper->reset();
        return r->provider()->uploadFileRequest(*directory, filename,
                                                stream_wrapper->prefix_,
                                                stream_wrapper->suffix_);
      },
      [=](EitherError<Response> e) {
        if (e.left()) return r->done(e.left());
        try {
          r->done(r->provider()->uploadFileResponse(*directory, filename,
                                                    stream_wrapper->size_,
                                                    e.right()->output()));
        } catch (const std::exception&) {
          r->done(Error{IHttpRequest::Failure, e.right()->output().str()});
        }
      },
      [=] { return std::make_shared<std::iostream>(stream_wrapper.get()); },
      std::make_shared<std::stringstream>(), nullptr,
      std::bind(&UploadFileRequest::ICallback::progress, callback, _1, _2),
      true);
}

UploadStreamWrapper::UploadStreamWrapper(
    std::function<uint32_t(char*, uint32_t, uint64_t)> callback, uint64_t size)
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
    uint32_t size =
        callback_(buffer_ + read_data, BUFFER_SIZE - read_data, read_);
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
