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

#include "IHttp.h"
#include "IRequest.h"
#include "Utility/Utility.h"

namespace cloudstorage {

class CloudProvider;
class HttpCallback;
class IItem;

template <class ReturnValue>
class Request : public IRequest<ReturnValue> {
 public:
  using ProgressFunction = std::function<void(uint32_t, uint32_t)>;
  using Resolver = std::function<ReturnValue(Request*)>;

  Request(std::shared_ptr<CloudProvider>);
  Request(std::weak_ptr<CloudProvider>);
  ~Request();

  void set_resolver(Resolver);

  void finish() override;
  void cancel() override;
  ReturnValue result() override;

  /**
   * If there is authorization in progress for the cloud provider, waits until
   * it finishes and returns its status; otherwise starts authorization itself
   * and waits for it to finish.
   *
   * @return whether the authorization was successful
   */
  EitherError<void> reauthorize();

  /**
   * Sends a request created by factory function; if request failed, tries to do
   * authorization and does the request again.
   *
   * @param factory function which should create request to perform
   * @param output output stream
   * @param error_stream error stream
   * @param download download progress callback
   * @param upload upload progress callback
   * @return http code or curl error code
   */
  int sendRequest(std::function<IHttpRequest::Pointer(std::ostream&)> factory,
                  std::ostream& output, Error* error_stream = nullptr,
                  ProgressFunction download = nullptr,
                  ProgressFunction upload = nullptr);

  int send(IHttpRequest*, std::istream& input, std::ostream& output,
           std::ostream* error, ProgressFunction download = nullptr,
           ProgressFunction upload = nullptr);

  std::shared_ptr<CloudProvider> provider() const;

  bool is_cancelled() { return is_cancelled_; }

  void subrequest(std::shared_ptr<IGenericRequest>);

 private:
  void set_cancelled(bool e) { is_cancelled_ = e; }

  std::unique_ptr<HttpCallback> httpCallback(
      ProgressFunction progress_download = nullptr,
      ProgressFunction progress_upload = nullptr);

  std::shared_ptr<CloudProvider> provider_shared_;
  std::weak_ptr<CloudProvider> provider_weak_;
  std::atomic_bool is_cancelled_;
  std::shared_future<ReturnValue> function_;
  std::mutex subrequest_mutex_;
  std::vector<std::shared_ptr<IGenericRequest>> subrequests_;
};

}  // namespace cloudstorage

#endif  // REQUEST_H
