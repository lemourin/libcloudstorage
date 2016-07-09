/*****************************************************************************
 * UploadFileRequest.cpp : UploadFileRequest implementation
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

#include "UploadFileRequest.h"

#include "CloudProvider/CloudProvider.h"
#include "Utility/HttpRequest.h"

namespace cloudstorage {

UploadFileRequest::UploadFileRequest(
    std::shared_ptr<CloudProvider> p, IItem::Pointer directory,
    const std::string& filename, UploadFileRequest::ICallback::Pointer callback)
    : Request(p),
      directory_(std::move(directory)),
      filename_(filename),
      stream_wrapper_(std::move(callback)) {
  if (!stream_wrapper_.callback_)
    throw std::logic_error("Callback can't be null.");
  function_ = std::async(std::launch::async, [this]() {
    std::stringstream response_stream;
    int code = sendRequest(
        [this](std::ostream& input) {
          stream_wrapper_.callback_->reset();
          std::istream stream(&stream_wrapper_);
          return provider()->uploadFileRequest(*directory_, filename_, stream,
                                               input);
        },
        response_stream, nullptr,
        std::bind(&ICallback::progress, stream_wrapper_.callback_.get(),
                  std::placeholders::_1, std::placeholders::_2));
    if (HttpRequest::isSuccess(code)) stream_wrapper_.callback_->done();
  });
}

UploadFileRequest::~UploadFileRequest() { cancel(); }

void UploadFileRequest::finish() {
  if (function_.valid()) function_.get();
}

void UploadFileRequest::error(int code, const std::string& description) {
  if (stream_wrapper_.callback_)
    stream_wrapper_.callback_->error(std::to_string(code) + ": " + description);
}

UploadFileRequest::UploadStreamWrapper::UploadStreamWrapper(
    UploadFileRequest::ICallback::Pointer callback)
    : callback_(std::move(callback)) {}

std::streambuf::int_type UploadFileRequest::UploadStreamWrapper::underflow() {
  uint32_t size = callback_->putData(buffer_, BUFFER_SIZE);
  if (gptr() == egptr()) setg(buffer_, buffer_, buffer_ + size);
  return gptr() == egptr() ? std::char_traits<char>::eof()
                           : std::char_traits<char>::to_int_type(*gptr());
}

}  // namespace cloudstorage
