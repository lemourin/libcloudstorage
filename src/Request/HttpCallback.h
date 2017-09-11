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
  using ProgressFunction = std::function<void(uint64_t, uint64_t)>;

  HttpCallback(std::function<bool()> is_cancelled,
               std::function<bool(int, const IHttpRequest::HeaderParameters&)>
                   is_success,
               ProgressFunction progress_download,
               ProgressFunction progress_upload);

  bool isSuccess(int, const IHttpRequest::HeaderParameters&) const override;

  bool abort() override;

  void progressDownload(uint64_t total, uint64_t now) override;

  void progressUpload(uint64_t, uint64_t) override;

 private:
  std::function<bool()> is_cancelled_;
  std::function<bool(int, const IHttpRequest::HeaderParameters&)> is_success_;
  ProgressFunction progress_download_;
  ProgressFunction progress_upload_;
};
}  // namespace cloudstorage

#endif  // HTTPCALLBACK_H
