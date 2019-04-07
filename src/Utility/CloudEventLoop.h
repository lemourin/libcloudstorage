/*****************************************************************************
 * CloudEventLoop.h
 *
 *****************************************************************************
 * Copyright (C) 2019 VideoLAN
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
#ifndef CLOUDEVENTLOOP_H
#define CLOUDEVENTLOOP_H

#include <atomic>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include "ICloudFactory.h"
#include "IRequest.h"
#include "IThreadPool.h"
#include "Promise.h"

namespace cloudstorage {

class Exception : public IException {
 public:
  Exception(int code, const std::string& description);
  Exception(const std::shared_ptr<Error>&);

  const char* what() const noexcept override { return description_.c_str(); }
  int code() const override { return code_; }

 private:
  int code_;
  std::string description_;
};

class CloudEventLoop;

namespace priv {

template <class>
class IFunction;

template <class Ret, class... Args>
class IFunction<Ret(Args...)> {
 public:
  using Pointer = std::unique_ptr<IFunction>;

  virtual Ret operator()(Args&&...) const = 0;
};

template <class, class>
class FunctionImpl;

template <class Func, class Ret, class... Args>
class FunctionImpl<Ret(Args...), Func> : public IFunction<Ret(Args...)> {
 public:
  FunctionImpl(Func&& f) : f_(std::move(f)) {}

  Ret operator()(Args&&... args) const override {
    return f_(std::forward<Args>(args)...);
  }

 private:
  Func f_;
};

template <class>
class Function;

template <class Ret, class... Args>
class Function<Ret(Args...)> {
 public:
  template <class Func>
  Function(Func&& f)
      : func_(
            std::make_unique<FunctionImpl<Ret(Args...), Func>>(std::move(f))) {}

  Ret operator()(Args&&... args) const {
    return (*func_)(std::forward<Args>(args)...);
  }

 private:
  typename IFunction<Ret(Args...)>::Pointer func_;
};

class LoopImpl {
 public:
  LoopImpl(CloudEventLoop*);

  void add(uint64_t tag, const std::shared_ptr<IGenericRequest>&);
  void fulfill(uint64_t tag, Function<void()>&&);
  void invoke(Function<void()>&&);

#ifdef WITH_THUMBNAILER
  void invokeOnThreadPool(Function<void()>&&);
#endif

  void clear();
  std::shared_ptr<std::atomic_bool> interrupt() const { return interrupt_; }

  uint64_t next_tag();
  void process_events();

 private:
  std::mutex mutex_;
  std::unordered_map<uint64_t, std::shared_ptr<IGenericRequest>> pending_;
  std::atomic_uint64_t last_tag_;
  std::vector<Function<void()>> events_;
#ifdef WITH_THUMBNAILER
  std::mutex thumbnailer_mutex_;
  IThreadPool::Pointer thumbnailer_thread_pool_;
#endif
  std::shared_ptr<std::atomic_bool> interrupt_;
  CloudEventLoop* event_loop_;
};

}  // namespace priv

template <class T>
using Function = priv::Function<T>;

class CloudEventLoop {
 public:
  CloudEventLoop(const std::shared_ptr<ICloudFactory::ICallback>& cb);
  ~CloudEventLoop();

  std::shared_ptr<priv::LoopImpl> impl() { return impl_; }

  void onEventAdded();
  void processEvents();

 private:
  std::shared_ptr<ICloudFactory::ICallback> callback_;
  std::shared_ptr<priv::LoopImpl> impl_;
};

}  // namespace cloudstorage

#endif  // CLOUDEVENTLOOP_H
