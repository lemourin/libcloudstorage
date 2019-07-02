/*****************************************************************************
 * HttpCallback.cpp : HttpCallback implementation
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

#include "HttpCallback.h"

#include "Request.h"

namespace cloudstorage {

HttpCallback::HttpCallback(
    std::function<int()> status,
    std::function<bool(int, const IHttpRequest::HeaderParameters&)> is_success,
    ProgressFunction progress_download, ProgressFunction progress_upload)
    : status_(std::move(status)),
      is_success_(std::move(is_success)),
      progress_download_(std::move(progress_download)),
      progress_upload_(std::move(progress_upload)) {}

bool HttpCallback::isSuccess(int code,
                             const IHttpRequest::HeaderParameters& h) const {
  return is_success_(code, h);
}

bool HttpCallback::abort() { return status_() == Request<int>::Cancelled; }

bool HttpCallback::pause() { return status_() == Request<int>::Paused; }

void HttpCallback::progressDownload(uint64_t total, uint64_t now) {
  if (progress_download_) progress_download_(total, now);
}

void HttpCallback::progressUpload(uint64_t total, uint64_t now) {
  if (progress_upload_) progress_upload_(total, now);
}

}  // namespace cloudstorage
