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
#include <list>
#include <unordered_map>
#include <unordered_set>
#include "ICloudFactory.h"
#include "IRequest.h"
#include "IThreadPool.h"
#include "Promise.h"

namespace cloudstorage {

class Exception : public IException {
 public:
  Exception(int code, std::string description);
  Exception(const std::shared_ptr<Error>&);

  const char* what() const noexcept override { return description_.c_str(); }
  int code() const override { return code_; }

 private:
  int code_;
  std::string description_;
};

class CloudEventLoop;

namespace priv {

class LoopImpl {
 public:
  LoopImpl(IThreadPoolFactory* factory, CloudEventLoop*);

  void add(uintptr_t tag, const std::shared_ptr<IGenericRequest>&);
  void fulfill(uintptr_t tag, std::function<void()>&&);
  void cancel(uintptr_t tag);
  void invoke(std::function<void()>&&);

#ifdef WITH_THUMBNAILER
  void invokeOnThreadPool(std::function<void()>&&);
  std::shared_ptr<IThreadPool> thumbnailer_thread_pool() const;
#endif

  void clear();
  std::shared_ptr<std::atomic_bool> interrupt() const { return interrupt_; }

  void process_events();

 private:
  std::mutex mutex_;
  std::unordered_map<uint64_t, std::shared_ptr<IGenericRequest>> pending_;
  std::list<std::function<void()>> events_;
#ifdef WITH_THUMBNAILER
  std::mutex thumbnailer_mutex_;
  std::shared_ptr<IThreadPool> thumbnailer_thread_pool_;
#endif
  IThreadPool::Pointer cancellation_thread_pool_;
  std::shared_ptr<std::atomic_bool> interrupt_;
  CloudEventLoop* event_loop_;
};

}  // namespace priv

class CloudEventLoop {
 public:
  CloudEventLoop(IThreadPoolFactory* factory,
                 std::shared_ptr<ICloudFactory::ICallback> cb);
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
