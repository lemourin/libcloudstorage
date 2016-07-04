/*****************************************************************************
 * DownloadFileRequest.cpp : DownloadFileRequest implementation
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

#include "DownloadFileRequest.h"

#include "CloudProvider.h"
#include "HttpCallback.h"
#include "HttpRequest.h"

namespace cloudstorage {

DownloadFileRequest::DownloadFileRequest(std::shared_ptr<CloudProvider> p,
                                         IItem::Pointer file,
                                         ICallback::Pointer callback)
    : Request(p), file_(std::move(file)), stream_wrapper_(std::move(callback)) {
  if (!stream_wrapper_.callback_)
    throw std::logic_error("Callback can't be null.");
  function_ = std::async(std::launch::async, [this]() {
    try {
      std::stringstream error_stream;
      int code = download(error_stream);
      if (HttpRequest::isAuthorizationError(code)) {
        if (!reauthorize()) {
          stream_wrapper_.callback_->error("Failed to authorize.");
          return;
        }
        code = download(error_stream);
      }
      if (HttpRequest::isClientError(code))
        stream_wrapper_.callback_->error("HTTP code " + std::to_string(code) +
                                         ": " + error_stream.str());
      else
        stream_wrapper_.callback_->done();
    } catch (const HttpException& e) {
      stream_wrapper_.callback_->error(e.what());
    }
  });
}

DownloadFileRequest::~DownloadFileRequest() { cancel(); }

void DownloadFileRequest::finish() {
  if (function_.valid()) function_.wait();
}

int DownloadFileRequest::download(std::ostream& error_stream) {
  std::stringstream stream;
  HttpRequest::Pointer request =
      provider()->downloadFileRequest(*file_, stream);
  provider()->authorizeRequest(*request);
  std::ostream response_stream(&stream_wrapper_);
  return request->send(
      stream, response_stream, &error_stream,
      httpCallback(std::bind(&DownloadFileRequest::ICallback::progress,
                             stream_wrapper_.callback_.get(),
                             std::placeholders::_1, std::placeholders::_2)));
}

DownloadFileRequest::DownloadStreamWrapper::DownloadStreamWrapper(
    DownloadFileRequest::ICallback::Pointer callback)
    : callback_(std::move(callback)) {}

std::streamsize DownloadFileRequest::DownloadStreamWrapper::xsputn(
    const char_type* data, std::streamsize length) {
  callback_->receivedData(data, static_cast<uint32_t>(length));
  return length;
}

}  // namespace cloudstorage
