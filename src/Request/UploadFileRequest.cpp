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
#include "HttpCallback.h"
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
    try {
      std::stringstream error_stream;
      int code = upload(error_stream);
      if (HttpRequest::isAuthorizationError(code)) {
        stream_wrapper_.callback_->reset();
        if (!reauthorize()) {
          stream_wrapper_.callback_->error("Failed to authorize.");
          return;
        }
        code = upload(error_stream);
      }
      if (HttpRequest::isClientError(code))
        stream_wrapper_.callback_->error(error_stream.str());
      else if (HttpRequest::isSuccess(code))
        stream_wrapper_.callback_->done();
    } catch (const HttpException& e) {
      stream_wrapper_.callback_->error(e.what());
    }
  });
}

UploadFileRequest::~UploadFileRequest() { cancel(); }

void UploadFileRequest::finish() {
  if (function_.valid()) function_.get();
}

int UploadFileRequest::upload(std::ostream& error_stream) {
  std::istream upload_data_stream(&stream_wrapper_);
  std::stringstream request_data;
  HttpRequest::Pointer request = provider()->uploadFileRequest(
      *directory_, filename_, upload_data_stream, request_data);
  provider()->authorizeRequest(*request);
  std::stringstream response;
  return request->send(
      request_data, response, &error_stream,
      httpCallback(
          nullptr,
          std::bind(&ICallback::progress, stream_wrapper_.callback_.get(),
                    std::placeholders::_1, std::placeholders::_2)));
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
