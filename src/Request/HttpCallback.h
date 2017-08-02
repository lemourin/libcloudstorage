/*****************************************************************************
 * HttpCallback.h : HttpCallback headers
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

#ifndef HTTPCALLBACK_H
#define HTTPCALLBACK_H

#include <atomic>
#include <functional>

#include "IHttp.h"

namespace cloudstorage {
class HttpCallback : public IHttpRequest::ICallback {
 public:
  HttpCallback(std::atomic_bool& is_cancelled,
               std::function<void(uint32_t, uint32_t)> progress_download,
               std::function<void(uint32_t, uint32_t)> progress_upload);

  bool abort() override;

  void progressDownload(uint32_t total, uint32_t now) override;

  void progressUpload(uint32_t, uint32_t) override;

  void receivedHttpCode(int) override;

  void receivedContentLength(int) override;

 private:
  std::atomic_bool& is_cancelled_;
  std::function<void(uint32_t, uint32_t)> progress_download_;
  std::function<void(uint32_t, uint32_t)> progress_upload_;
};
}  // namespace cloudstorage

#endif  // HTTPCALLBACK_H
