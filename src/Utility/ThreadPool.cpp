/*****************************************************************************
 * ThreadPool.cpp
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
#include "ThreadPool.h"

#include <algorithm>

#include "Utility/Utility.h"

namespace cloudstorage {

ThreadPool::ThreadPool(uint32_t thread_count)
    : destroyed_(false),
      delayed_tasks_thread_(std::bind(&ThreadPool::handleDelayedTasks, this)) {
  for (uint32_t i = 0; i < thread_count; ++i) {
    workers_.emplace_back([this]() {
      util::set_thread_name("cs-threadpool");
      util::attach_thread();
      while (true) {
        Task task;
        {
          std::unique_lock<std::mutex> lock(mutex_);
          if (destroyed_ && tasks_.empty()) {
            break;
          }
          while (tasks_.empty() && !destroyed_) {
            worker_cv_.wait(lock);
          }
          if (!tasks_.empty()) {
            task = tasks_.front();
            tasks_.pop_front();
          } else {
            break;
          }
        }
        task();
      }
      util::detach_thread();
    });
  }
}

ThreadPool::~ThreadPool() {
  std::unique_lock<std::mutex> lock(mutex_);
  destroyed_ = true;
  worker_cv_.notify_all();
  for (auto &worker : workers_) {
    lock.unlock();
    worker.join();
    lock.lock();
  }
  lock.unlock();
  delayed_tasks_thread_.join();
  lock.lock();
}

void ThreadPool::schedule(const Task &f,
                          const std::chrono::system_clock::time_point &when) {
  std::unique_lock<std::mutex> lock(mutex_);
  if (when <= std::chrono::system_clock::now()) {
    tasks_.emplace_back(f);
  } else {
    delayed_tasks_.insert({when, f});
  }
  worker_cv_.notify_all();
}

void ThreadPool::handleDelayedTasks() {
  util::set_thread_name("cs-threadpool");
  util::attach_thread();
  while (true) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (destroyed_ && delayed_tasks_.empty()) {
      break;
    }
    while (delayed_tasks_.empty() && !destroyed_) {
      worker_cv_.wait_for(lock, std::chrono::milliseconds(100));
    }
    if (!delayed_tasks_.empty()) {
      if (delayed_tasks_.begin()->first <= std::chrono::system_clock::now() ||
          destroyed_) {
        auto task = *delayed_tasks_.begin();
        delayed_tasks_.erase(delayed_tasks_.begin());
        lock.unlock();
        task.second();
        lock.lock();
      }
    }
  }
  util::detach_thread();
}

IThreadPool::Pointer IThreadPool::create(uint32_t threads) {
  return util::make_unique<ThreadPool>(threads);
}

}  // namespace cloudstorage
