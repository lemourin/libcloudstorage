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
class Request : public IRequest<ReturnValue>,
                public std::enable_shared_from_this<Request<ReturnValue>> {
 public:
  using Pointer = std::shared_ptr<Request<ReturnValue>>;
  using ProgressFunction = std::function<void(uint32_t, uint32_t)>;
  using RequestFactory =
      std::function<IHttpRequest::Pointer(std::shared_ptr<std::ostream>)>;
  using Resolver = std::function<void(std::shared_ptr<Request>)>;
  using AuthorizeCompleted = std::function<void(EitherError<void>)>;
  using RequestCompleted = std::function<void(EitherError<util::Output>)>;

  class Wrapper : public IRequest<ReturnValue> {
   public:
    Wrapper(typename Request<ReturnValue>::Pointer);
    ~Wrapper();

    void finish() override;
    void cancel() override;
    ReturnValue result() override;

   private:
    typename Request<ReturnValue>::Pointer request_;
  };

  Request(std::shared_ptr<CloudProvider>);
  ~Request();

  void finish() override;
  void cancel() override;
  ReturnValue result() override;

  void set(Resolver);
  typename Wrapper::Pointer run();
  void done(const ReturnValue&);

  /**
   * If there is authorization in progress for the cloud provider, waits until
   * it finishes and returns its status; otherwise starts authorization itself
   * and waits for it to finish.
   *
   * @return whether the authorization was successful
   */
  void reauthorize(AuthorizeCompleted);

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
  void sendRequest(RequestFactory factory, RequestCompleted,
                   std::shared_ptr<std::ostream> output,
                   ProgressFunction download = nullptr,
                   ProgressFunction upload = nullptr);

  void send(IHttpRequest*, IHttpRequest::CompleteCallback complete,
            std::shared_ptr<std::istream> input,
            std::shared_ptr<std::ostream> output,
            std::shared_ptr<std::ostream> error,
            ProgressFunction download = nullptr,
            ProgressFunction upload = nullptr);

  std::shared_ptr<CloudProvider> provider() const;

  bool is_cancelled() const;

  void subrequest(std::shared_ptr<IGenericRequest>);

  void authorize(IHttpRequest::Pointer r);
  bool reauthorize(int code);

 private:
  friend class AuthorizeRequest;

  std::unique_ptr<HttpCallback> httpCallback(
      ProgressFunction progress_download = nullptr,
      ProgressFunction progress_upload = nullptr);

  std::promise<ReturnValue> value_;
  std::shared_future<ReturnValue> future_;
  Resolver resolver_;
  std::mutex provider_mutex_;
  std::shared_ptr<CloudProvider> provider_;
  std::atomic_bool is_cancelled_;
  std::mutex subrequest_mutex_;
  std::vector<std::shared_ptr<IGenericRequest>> subrequests_;
};

}  // namespace cloudstorage

#endif  // REQUEST_H
