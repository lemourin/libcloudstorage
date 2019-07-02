/*****************************************************************************
 * DownloadFileRequest.h : DownloadFileRequest headers
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

#ifndef DOWNLOADFILEREQUEST_H
#define DOWNLOADFILEREQUEST_H

#include "IItem.h"
#include "Request.h"

namespace cloudstorage {

class DownloadStreamWrapper : public std::streambuf {
 public:
  DownloadStreamWrapper(std::function<void(const char*, uint32_t)> callback);
  std::streamsize xsputn(const char_type* data,
                         std::streamsize length) override;

 private:
  std::function<void(const char*, uint32_t)> callback_;
};

class DownloadFileRequest : public Request<EitherError<void>> {
 public:
  using RequestFactory =
      std::function<IHttpRequest::Pointer(const IItem&, std::ostream&)>;
  using ICallback = IDownloadFileCallback;

  DownloadFileRequest(std::shared_ptr<CloudProvider>,
                      const IItem::Pointer& file, const ICallback::Pointer&,
                      Range, const RequestFactory& request_factory);
  ~DownloadFileRequest() override;

 private:
  void resolve(const Request::Pointer& request, const IItem::Pointer& file,
               ICallback*, Range, const RequestFactory& request_factory);

  DownloadStreamWrapper stream_wrapper_;
};

class DownloadFileFromUrlRequest : public Request<EitherError<void>> {
 public:
  using ICallback = IDownloadFileCallback;

  DownloadFileFromUrlRequest(std::shared_ptr<CloudProvider>, IItem::Pointer,
                             const ICallback::Pointer&, Range);
  ~DownloadFileFromUrlRequest() override;

 private:
  void resolve(const Request::Pointer&, const IItem::Pointer&, ICallback*,
               Range);

  DownloadStreamWrapper stream_wrapper_;
};

}  // namespace cloudstorage

#endif  // DOWNLOADFILEREQUEST_H
