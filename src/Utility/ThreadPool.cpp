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

#include "Utility/Utility.h"
#include <algorithm>

namespace cloudstorage {

ThreadPool::ThreadPool(uint32_t thread_count) : destroyed_(false) {
  for (uint32_t i = 0; i < thread_count; ++i) {
    workers_.emplace_back([this]() {
      while (true) {
        Task task;
        {
          std::unique_lock<std::mutex> lock(mutex_);
          if (destroyed_) {
            break;
          }
          while (tasks_.empty() && !destroyed_) {
            worker_cv_.wait(lock);
          }
          if (!tasks_.empty()) {
            task = std::move(tasks_.front());
            tasks_.pop();
          } else {
            break;
          }
        }
        task();
      }
    });
  }
}

ThreadPool::~ThreadPool() {
  std::unique_lock<std::mutex> lock(mutex_);
  destroyed_ = true;
  worker_cv_.notify_all();
  for (auto &worker : workers_) {
    worker.join();
  }
}

void ThreadPool::schedule(const Task &f) {
  std::unique_lock<std::mutex> lock(mutex_);
  tasks_.emplace(std::move(f));
  if (tasks_.size() == 1) {
    worker_cv_.notify_one();
  }
}

IThreadPool::Pointer IThreadPool::create(uint32_t threads) {
  return util::make_unique<ThreadPool>(threads);
}

} // namespace cloudstorage
