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

#include "CloudProvider/CloudProvider.h"
#include "HttpCallback.h"
#include "Utility/HttpRequest.h"

namespace cloudstorage {

DownloadFileRequest::DownloadFileRequest(std::shared_ptr<CloudProvider> p,
                                         IItem::Pointer file,
                                         ICallback::Pointer callback,
                                         RequestFactory request_factory)
    : Request(p),
      file_(std::move(file)),
      stream_wrapper_(std::bind(&ICallback::receivedData, callback.get(),
                                std::placeholders::_1, std::placeholders::_2)),
      callback_(std::move(callback)),
      request_factory_(request_factory) {
  function_ = std::async(std::launch::async, [this]() {
    try {
      std::stringstream error_stream;
      int code = download(error_stream);
      if (HttpRequest::isAuthorizationError(code)) {
        if (!reauthorize()) {
          callback_->error("Failed to authorize.");
          return;
        }
        code = download(error_stream);
      }
      if (HttpRequest::isClientError(code))
        callback_->error("HTTP code " + std::to_string(code) + ": " +
                         error_stream.str());
      else if (HttpRequest::isSuccess(code))
        callback_->done();
    } catch (const HttpException& e) {
      callback_->error(e.what());
    }
  });
}

DownloadFileRequest::~DownloadFileRequest() { cancel(); }

void DownloadFileRequest::finish() {
  if (function_.valid()) function_.wait();
}

int DownloadFileRequest::download(std::ostream& error_stream) {
  std::stringstream stream;
  HttpRequest::Pointer request = request_factory_(*file_, stream);
  provider()->authorizeRequest(*request);
  std::ostream response_stream(&stream_wrapper_);
  return request->send(
      stream, response_stream, &error_stream,
      httpCallback(std::bind(&DownloadFileRequest::ICallback::progress,
                             callback_.get(), std::placeholders::_1,
                             std::placeholders::_2)));
}

DownloadStreamWrapper::DownloadStreamWrapper(
    std::function<void(const char*, uint32_t)> callback)
    : callback_(std::move(callback)) {}

std::streamsize DownloadStreamWrapper::xsputn(const char_type* data,
                                              std::streamsize length) {
  callback_(data, static_cast<uint32_t>(length));
  return length;
}

}  // namespace cloudstorage
