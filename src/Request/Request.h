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

class Response {
 public:
  Response(IHttpRequest::Response);

  int http_code() const;
  const IHttpRequest::HeaderParameters& headers() const;
  std::stringstream& output();
  std::stringstream& error_output();

 private:
  IHttpRequest::Response http_;
};

template <class ReturnValue>
class Request : public IRequest<ReturnValue>,
                public std::enable_shared_from_this<Request<ReturnValue>> {
 public:
  using Pointer = std::shared_ptr<Request<ReturnValue>>;
  using ProgressFunction = std::function<void(uint64_t, uint64_t)>;
  using RequestFactory =
      std::function<IHttpRequest::Pointer(std::shared_ptr<std::ostream>)>;
  using InputFactory = std::function<std::shared_ptr<std::iostream>()>;
  using Callback = std::function<void(ReturnValue)>;
  using Resolver = std::function<void(std::shared_ptr<Request>)>;
  using AuthorizeCompleted = std::function<void(EitherError<void>)>;
  using RequestCompleted = std::function<void(EitherError<Response>)>;

  enum Status { None = 0, Cancelled = 1, Paused = 2 };

  class Wrapper : public IRequest<ReturnValue> {
   public:
    Wrapper(typename Request<ReturnValue>::Pointer);
    ~Wrapper() override;

    void finish() override;
    void cancel() override;
    ReturnValue result() override;
    void pause() override;
    void resume() override;

   private:
    typename Request<ReturnValue>::Pointer request_;
  };

  Request(std::shared_ptr<CloudProvider>, Callback, Resolver);
  ~Request() override;

  void finish() override;
  void cancel() override;
  ReturnValue result() override;
  void pause() override;
  void resume() override;

  typename Wrapper::Pointer run();
  void done(const ReturnValue&);

  void reauthorize(const AuthorizeCompleted&);

  void send(const RequestFactory& factory, const RequestCompleted&,
            const InputFactory&, const std::shared_ptr<std::ostream>& output,
            const ProgressFunction& download, const ProgressFunction& upload,
            bool authorized);

  void request(const RequestFactory& factory, const RequestCompleted&);
  void send(const RequestFactory& factory, const RequestCompleted&);
  void query(const RequestFactory& factory,
             const IHttpRequest::CompleteCallback&);

  std::shared_ptr<CloudProvider> provider() const;

  bool is_cancelled() const;
  bool is_paused() const;

  template <class Type = CloudProvider, class Method, class... Args>
  void make_subrequest(Method method, Args... args) {
    if (is_cancelled()) {
      call(LastArgument<Args...>()(args...),
           Error{IHttpRequest::Aborted, util::Error::ABORTED});
    } else {
      auto r = (static_cast<Type*>(provider().get())->*method)(
          std::forward<Args>(args)...);
      std::lock_guard<std::mutex> lock(subrequest_mutex_);
      subrequests_.emplace_back(std::move(r));
    }
  }

  void authorize(const IHttpRequest::Pointer& r);
  bool reauthorize(int code, const IHttpRequest::HeaderParameters&);

 private:
  friend class AuthorizeRequest;

  std::unique_ptr<HttpCallback> http_callback(
      const ProgressFunction& progress_download = nullptr,
      const ProgressFunction& progress_upload = nullptr);

  void send(IHttpRequest*, const IHttpRequest::CompleteCallback& complete,
            const std::shared_ptr<std::istream>& input,
            const std::shared_ptr<std::ostream>& output,
            const std::shared_ptr<std::ostream>& error,
            const ProgressFunction& download = nullptr,
            const ProgressFunction& upload = nullptr);

  void subrequest(std::shared_ptr<IGenericRequest>);

  template <class First, class... Rest>
  struct LastArgument {
    using Type = typename LastArgument<Rest...>::Type;

    const Type& operator()(const First&, const Rest&... rest) const {
      return LastArgument<Rest...>()(rest...);
    }
  };

  template <class First>
  struct LastArgument<First> {
    using Type = First;

    const Type& operator()(const First& f) const { return f; }
  };

  template <class T, class... Args>
  void call(const std::unique_ptr<T>& c, Args... args) {
    c->done(std::forward<Args>(args)...);
  }

  template <class T, class... Args>
  void call(const std::shared_ptr<T>& c, Args... args) {
    c->done(std::forward<Args>(args)...);
  }

  template <class T, class... Args>
  void call(const T& c, Args... args) {
    c(std::forward<Args>(args)...);
  }

  std::promise<ReturnValue> value_;
  std::shared_future<ReturnValue> future_;
  Resolver resolver_;
  Callback callback_;
  std::mutex provider_mutex_;
  std::shared_ptr<CloudProvider> provider_;
  mutable std::mutex status_mutex_;
  Status status_;
  std::mutex subrequest_mutex_;
  std::vector<std::shared_ptr<IGenericRequest>> subrequests_;
};

}  // namespace cloudstorage

#endif  // REQUEST_H
