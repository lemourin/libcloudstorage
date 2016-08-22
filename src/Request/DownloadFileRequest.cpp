/*****************************************************************************
 * DownloadFileRequest.cpp : DownloadFileRequest implementation
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

#include "DownloadFileRequest.h"

#include "CloudProvider/CloudProvider.h"

using namespace std::placeholders;

namespace cloudstorage {

DownloadFileRequest::DownloadFileRequest(std::shared_ptr<CloudProvider> p,
                                         IItem::Pointer file,
                                         ICallback::Pointer callback,
                                         RequestFactory request_factory,
                                         bool fallback_thumbnail)
    : Request(p),
      file_(std::move(file)),
      stream_wrapper_(
          std::bind(&ICallback::receivedData, callback.get(), _1, _2)),
      callback_(std::move(callback)),
      request_factory_(request_factory),
      fallback_thumbnail_(fallback_thumbnail) {
  set_resolver([this](Request* r) {
    std::ostream response_stream(&stream_wrapper_);
    int code = sendRequest(
        [this](std::ostream& input) { return request_factory_(*file_, input); },
        response_stream, std::bind(&DownloadFileRequest::ICallback::progress,
                                   callback_.get(), _1, _2));
    if (IHttpRequest::isSuccess(code))
      callback_->done();
    else if (fallback_thumbnail_)
      generateThumbnail(r, file_, callback_);
  });
}

DownloadFileRequest::~DownloadFileRequest() { cancel(); }

void DownloadFileRequest::generateThumbnail(
    Request<void>* r, IItem::Pointer item,
    IDownloadFileCallback::Pointer callback) {
  if (!r->provider()->thumbnailer()) return;
  Request<void>::Semaphore semaphore(r);
  auto item_data = r->provider()->getItemDataAsync(
      item->id(), [&semaphore](IItem::Pointer) { semaphore.notify(); });
  semaphore.wait();
  if (r->is_cancelled()) return item_data->cancel();
  if (!item_data->result()) return callback->error("Couldn't get item url.");
  auto thumbnail_data = r->provider()->thumbnailer()->generateThumbnail(
      r->provider(), item_data->result(),
      [&semaphore, callback](const std::vector<char>& data) {
        if (!data.empty()) {
          callback->receivedData(data.data(), data.size());
          callback->done();
        }
        semaphore.notify();
      },
      [callback](const std::string& error) {
        callback->error("Thumbnailer error: " + error);
      });
  semaphore.wait();
  if (r->is_cancelled()) thumbnail_data->cancel();
}

void DownloadFileRequest::error(int code, const std::string& description) {
  if (!fallback_thumbnail_) callback_->error(error_string(code, description));
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
