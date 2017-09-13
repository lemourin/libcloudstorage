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
#include "Utility/Item.h"

using namespace std::placeholders;

namespace cloudstorage {

DownloadFileRequest::DownloadFileRequest(std::shared_ptr<CloudProvider> p,
                                         IItem::Pointer file,
                                         ICallback::Pointer cb, Range range,
                                         RequestFactory request_factory)
    : Request(p),
      stream_wrapper_(std::bind(&ICallback::receivedData, cb.get(), _1, _2)) {
  auto callback = cb.get();
  set(
      [=](Request::Pointer request) {
        sendRequest(
            [=](util::Output input) {
              auto request = request_factory(*file, *input);
              if (range != FullRange)
                request->setHeaderParameter("Range",
                                            util::range_to_string(range));
              return request;
            },
            [=](EitherError<Response> e) {
              if (e.left())
                request->done(e.left());
              else
                request->done(nullptr);
            },
            nullptr, std::make_shared<std::ostream>(&stream_wrapper_),
            std::bind(&DownloadFileRequest::ICallback::progress, callback, _1,
                      _2));
      },
      [=](EitherError<void> e) { cb->done(e); });
}

DownloadFileRequest::~DownloadFileRequest() { cancel(); }

DownloadStreamWrapper::DownloadStreamWrapper(
    std::function<void(const char*, uint32_t)> callback)
    : callback_(std::move(callback)) {}

std::streamsize DownloadStreamWrapper::xsputn(const char_type* data,
                                              std::streamsize length) {
  callback_(data, static_cast<uint32_t>(length));
  return length;
}

DownloadFileFromUrlRequest::DownloadFileFromUrlRequest(
    std::shared_ptr<CloudProvider> p, IItem::Pointer file,
    ICallback::Pointer cb, Range range)
    : Request(p),
      stream_wrapper_(std::bind(&ICallback::receivedData, cb.get(), _1, _2)) {
  auto callback = cb.get();
  set(
      [=](Request<EitherError<void>>::Pointer r) {
        auto download = [=](std::string url,
                            std::function<void(EitherError<void>)> cb) {
          r->sendRequest(
              [=](util::Output) {
                auto r = provider()->http()->create(url, "GET");
                if (range != FullRange)
                  r->setHeaderParameter("Range", util::range_to_string(range));
                return r;
              },
              [=](EitherError<Response> e) {
                if (e.left())
                  cb(e.left());
                else
                  cb(nullptr);
              },
              nullptr, std::make_shared<std::ostream>(&stream_wrapper_),
              std::bind(&IDownloadFileCallback::progress, callback, _1, _2));
        };
        auto cached_url = static_cast<Item*>(file.get())->url();
        auto get_url = [=]() {
          r->subrequest(provider()->getItemUrlAsync(
              file, [=](EitherError<std::string> e) {
                if (e.left()) return r->done(e.left());
                download(*e.right(), [=](EitherError<void> e) { r->done(e); });
              }));
        };
        if (!cached_url.empty())
          download(cached_url, [=](EitherError<void> e) {
            if (e.left())
              get_url();
            else
              r->done(e);
          });
        else
          get_url();
      },
      [=](EitherError<void> e) { cb->done(e); });
}

DownloadFileFromUrlRequest::~DownloadFileFromUrlRequest() { cancel(); }

}  // namespace cloudstorage
