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
#include "Utility/HttpRequest.h"

using namespace std::placeholders;

namespace cloudstorage {

UploadFileRequest::UploadFileRequest(
    std::shared_ptr<CloudProvider> p, IItem::Pointer directory,
    const std::string& filename, UploadFileRequest::ICallback::Pointer callback)
    : Request(p),
      directory_(std::move(directory)),
      filename_(filename),
      stream_wrapper_(std::bind(&ICallback::putData, callback.get(), _1, _2)),
      callback_(callback) {
  if (!stream_wrapper_.callback_)
    throw std::logic_error("Callback can't be null.");
  set_resolver([this](Request*) {
    std::stringstream response_stream;
    int code = sendRequest(
        [this](std::ostream& input) {
          callback_->reset();
          std::istream stream(&stream_wrapper_);
          return provider()->uploadFileRequest(*directory_, filename_, stream,
                                               input);
        },
        response_stream, nullptr,
        std::bind(&ICallback::progress, callback_.get(), _1, _2));
    if (HttpRequest::isSuccess(code)) callback_->done();
  });
}

UploadFileRequest::~UploadFileRequest() { cancel(); }

void UploadFileRequest::error(int code, const std::string& description) {
  if (callback_) callback_->error(error_string(code, description));
}

UploadStreamWrapper::UploadStreamWrapper(
    std::function<uint32_t(char*, uint32_t)> callback)
    : callback_(std::move(callback)) {}

std::streambuf::int_type UploadStreamWrapper::underflow() {
  uint32_t size = callback_(buffer_, BUFFER_SIZE);
  if (gptr() == egptr()) setg(buffer_, buffer_, buffer_ + size);
  return gptr() == egptr() ? std::char_traits<char>::eof()
                           : std::char_traits<char>::to_int_type(*gptr());
}

}  // namespace cloudstorage
