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
  using ErrorCallback = std::function<void(Request*, Error)>;
  using CancelCallback = std::function<void()>;

  Request(std::shared_ptr<CloudProvider>);
  Request(std::weak_ptr<CloudProvider>);
  ~Request();

  void set_resolver(Resolver);
  void set_error_callback(ErrorCallback);
  void set_cancel_callback(CancelCallback);

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
  bool reauthorize();

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
  virtual void error(int code, const std::string& description);
  std::string error_string(int code, const std::string& desc) const;

  bool is_cancelled() { return is_cancelled_; }

  class Semaphore : public cloudstorage::Semaphore {
   public:
    Semaphore(Request*);
    ~Semaphore();

   private:
    Request<ReturnValue>* request_;
  };

 private:
  void set_cancelled(bool e) { is_cancelled_ = e; }

  std::unique_ptr<HttpCallback> httpCallback(
      ProgressFunction progress_download = nullptr,
      ProgressFunction progress_upload = nullptr);

  std::shared_ptr<CloudProvider> provider_shared_;
  std::weak_ptr<CloudProvider> provider_weak_;
  std::atomic_bool is_cancelled_;
  std::shared_future<ReturnValue> function_;
  ErrorCallback error_callback_;
  CancelCallback cancel_callback_;
  std::mutex semaphore_list_mutex_;
  std::vector<Semaphore*> semaphore_list_;
};

}  // namespace cloudstorage

#endif  // REQUEST_H
