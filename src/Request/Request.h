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

namespace cloudstorage {

class CloudProvider;
class HttpRequest;
class HttpCallback;

class Request {
 public:
  using ProgressFunction = std::function<void(uint32_t, uint32_t)>;
  using Resolver = std::function<void(Request*)>;

  Request(std::shared_ptr<CloudProvider>);
  virtual ~Request() = default;

  void set_resolver(Resolver);

  virtual void finish();
  virtual void cancel();

  int sendRequest(
      std::function<std::shared_ptr<HttpRequest>(std::ostream&)> factory,
      std::ostream& output, ProgressFunction download = nullptr,
      ProgressFunction upload = nullptr);
  int send(HttpRequest*, std::istream& input, std::ostream& output,
           std::ostream* error, ProgressFunction download = nullptr,
           ProgressFunction upload = nullptr);

 protected:
  std::shared_ptr<CloudProvider> provider() const { return provider_; }
  virtual void error(int code, const std::string& description);
  std::string error_string(int code, const std::string& desc) const;

  void set_cancelled(bool e) { is_cancelled_ = e; }
  bool is_cancelled() { return is_cancelled_; }

 private:
  std::unique_ptr<HttpCallback> httpCallback(
      ProgressFunction progress_download = nullptr,
      ProgressFunction progress_upload = nullptr);
  bool reauthorize();

  std::shared_ptr<CloudProvider> provider_;
  std::atomic_bool is_cancelled_;
  std::shared_future<void> function_;
};

}  // namespace cloudstorage

#endif  // REQUEST_H
