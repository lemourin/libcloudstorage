/*****************************************************************************
 * Request.cpp : Request implementation
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

#include "Request.h"

#include <iostream>

#include "CloudProvider.h"
#include "Utility.h"

namespace cloudstorage {

Request::Request(CloudProvider* provider) : provider_(provider) {}

void Request::cancel() {
  // TODO
}

ListDirectoryRequest::ListDirectoryRequest(CloudProvider* p,
                                           IItem::Pointer directory,
                                           ICallback::Pointer callback)
    : Request(p),
      directory_(std::move(directory)),
      callback_(std::move(callback)) {
  result_ = std::async(std::launch::async, [this]() {
    std::vector<IItem::Pointer> result;
    std::string page_token;
    bool retry;
    do {
      retry = false;
      provider()->waitForAuthorized();
      HttpRequest::Pointer r = provider()->listDirectoryRequest(
          *directory_, page_token, input_stream(), provider()->access_token());
      std::stringstream output_stream;
      if (HttpRequest::isSuccess(r->send(input_stream(), output_stream))) {
        page_token = "";
        for (auto& t :
             provider()->listDirectoryResponse(output_stream, page_token)) {
          if (callback_)
            callback_->receivedItem(std::move(t));
          else
            result.push_back(std::move(t));
        }
      } else {
        if (!provider()->authorize()) throw AuthorizationException();
        retry = true;
      }
    } while (!page_token.empty() || retry);
    return result;
  });
}

std::vector<IItem::Pointer> ListDirectoryRequest::result() {
  if (!result_.valid()) throw std::logic_error("Future invalid.");
  return result_.get();
}

GetItemRequest::GetItemRequest(CloudProvider* p, const std::string& path,
                               std::function<void(IItem::Pointer)> callback)
    : Request(p), path_(path), callback_(callback) {
  result_ = std::async(std::launch::async, [this]() -> IItem::Pointer {
    if (path_.empty() || path_.front() != '/') return nullptr;
    IItem::Pointer node = provider()->rootDirectory();
    std::stringstream stream(path_.substr(1));
    std::string token;
    while (std::getline(stream, token, '/')) {
      if (!node || !node->is_directory()) return nullptr;
      node = getItem(
          provider()->listDirectoryAsync(std::move(node), nullptr)->result(),
          token);
    }
    if (callback_) {
      callback_(std::move(node));
      return nullptr;
    } else {
      return node;
    }
  });
}

IItem::Pointer GetItemRequest::result() {
  if (!result_.valid()) throw std::logic_error("Future invalid.");
  return result_.get();
}

IItem::Pointer GetItemRequest::getItem(std::vector<IItem::Pointer>&& items,
                                       const std::string& name) const {
  for (IItem::Pointer& i : items)
    if (i->filename() == name) return std::move(i);
  return nullptr;
}

DownloadFileRequest::DownloadFileRequest(CloudProvider* p, IItem::Pointer file,
                                         ICallback::Pointer callback)
    : Request(p), file_(std::move(file)), stream_wrapper_(std::move(callback)) {
  function_ = std::async(std::launch::async, [this]() {
    if (!download()) {
      stream_wrapper_.callback_->reset();
      if (!provider()->authorize()) throw AuthorizationException();
      if (!download()) throw std::logic_error("Invalid download request.");
    }
    if (stream_wrapper_.callback_) stream_wrapper_.callback_->done();
  });
}

void DownloadFileRequest::finish() { function_.get(); }

bool DownloadFileRequest::download() {
  provider()->waitForAuthorized();
  std::stringstream stream;
  HttpRequest::Pointer request = provider()->downloadFileRequest(
      *file_, stream, provider()->access_token());
  std::ostream response_stream(&stream_wrapper_);
  return HttpRequest::isSuccess(request->send(stream, response_stream));
}

DownloadFileRequest::DownloadStreamWrapper::DownloadStreamWrapper(
    DownloadFileRequest::ICallback::Pointer callback)
    : callback_(std::move(callback)) {}

std::streamsize DownloadFileRequest::DownloadStreamWrapper::xsputn(
    const char_type* data, std::streamsize length) {
  if (callback_) callback_->receivedData(data, length);
  return length;
}

UploadFileRequest::UploadFileRequest(
    CloudProvider* p, IItem::Pointer directory, const std::string& filename,
    UploadFileRequest::ICallback::Pointer callback)
    : Request(p),
      directory_(std::move(directory)),
      filename_(filename),
      stream_wrapper_(std::move(callback)) {
  function_ = std::async(std::launch::async, [this]() {
    if (!upload()) {
      stream_wrapper_.callback_->reset();
      if (!provider()->authorize()) throw AuthorizationException();
      if (!upload()) throw std::logic_error("Invalid upload request.");
    }
    stream_wrapper_.callback_->done();
  });
}

void UploadFileRequest::finish() { function_.wait(); }

bool UploadFileRequest::upload() {
  provider()->waitForAuthorized();
  std::istream upload_data_stream(&stream_wrapper_);
  std::stringstream request_data;
  HttpRequest::Pointer request =
      provider()->uploadFileRequest(*directory_, filename_, upload_data_stream,
                                    request_data, provider()->access_token());
  std::stringstream response;
  return HttpRequest::isSuccess(request->send(request_data, response));
}

UploadFileRequest::UploadStreamWrapper::UploadStreamWrapper(
    UploadFileRequest::ICallback::Pointer callback)
    : callback_(std::move(callback)) {}

std::streambuf::int_type UploadFileRequest::UploadStreamWrapper::underflow() {
  uint size = callback_->putData(buffer_, BUFFER_SIZE);
  if (gptr() == egptr()) setg(buffer_, buffer_, buffer_ + size);
  return gptr() == egptr() ? std::char_traits<char>::eof()
                           : std::char_traits<char>::to_int_type(*gptr());
}

}  // namespace cloudstorage
