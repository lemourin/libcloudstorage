/*****************************************************************************
 * Request.h : Request prototypes
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

#ifndef REQUEST_H
#define REQUEST_H

#include <future>
#include <mutex>
#include <sstream>
#include <vector>

#include "IItem.h"

namespace cloudstorage {

class AuthorizeRequest;
class CloudProvider;
class HttpRequest;
class HttpCallback;

class Request {
 public:
  Request(std::shared_ptr<CloudProvider>);
  virtual ~Request() = default;

  virtual void finish() = 0;
  virtual void cancel();

 protected:
  std::unique_ptr<HttpCallback> httpCallback(
      std::function<void(uint32_t, uint32_t)> progress_download = nullptr);
  std::stringstream& input_stream() { return input_stream_; }
  std::shared_ptr<CloudProvider> provider() const { return provider_; }
  bool reauthorize();

  void set_cancelled(bool e) { is_cancelled_ = e; }
  bool is_cancelled() { return is_cancelled_; }

 private:
  std::mutex mutex_;
  std::unique_ptr<AuthorizeRequest> authorize_request_;
  std::shared_ptr<CloudProvider> provider_;
  std::stringstream input_stream_;
  std::atomic_bool is_cancelled_;
};

}  // namespace cloudstorage

#endif  // REQUEST_H
