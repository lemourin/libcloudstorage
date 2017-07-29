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
  set_resolver([this](Request* r) -> EitherError<void> {
    std::ostream response_stream(&stream_wrapper_);
    Error error;
    int code = sendRequest(
        [this](std::ostream& input) { return request_factory_(*file_, input); },
        response_stream, &error,
        std::bind(&DownloadFileRequest::ICallback::progress, callback_.get(),
                  _1, _2));
    if (IHttpRequest::isSuccess(code)) {
      callback_->done(nullptr);
      return nullptr;
    } else if (fallback_thumbnail_) {
      generateThumbnail(r, file_, callback_);
      return nullptr;
    } else {
      return error;
    }
  });
}

DownloadFileRequest::~DownloadFileRequest() { cancel(); }

void DownloadFileRequest::generateThumbnail(
    Request<EitherError<void>>* r, IItem::Pointer item,
    IDownloadFileCallback::Pointer callback) {
  if (!r->provider()->thumbnailer()) return;
  Request<EitherError<void>>::Semaphore semaphore(r);
  auto item_data = r->provider()->getItemDataAsync(
      item->id(), [&semaphore](EitherError<IItem>) { semaphore.notify(); });
  semaphore.wait();
  if (r->is_cancelled()) return item_data->cancel();
  if (!item_data->result().right())
    return callback->done(item_data->result().left());
  auto thumbnail_data = r->provider()->thumbnailer()->generateThumbnail(
      r->provider(), item_data->result().right(),
      [&semaphore, callback](EitherError<std::vector<char>> data) {
        if (data.right()) {
          callback->receivedData(data.right()->data(), data.right()->size());
          callback->done(nullptr);
        } else {
          callback->done(data.left());
        }
        semaphore.notify();
      });
  semaphore.wait();
  if (r->is_cancelled()) thumbnail_data->cancel();
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
