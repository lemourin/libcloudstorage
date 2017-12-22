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

IThreadPool::Pointer IThreadPool::create(uint32_t cnt) {
  return util::make_unique<ThreadPool>(cnt);
}

ThreadPool::ThreadPool(uint32_t thread_count) : worker_(thread_count) {}

void ThreadPool::schedule(std::function<void()> f) {
  auto it = std::min_element(worker_.begin(), worker_.end(),
                             [](const Worker& w1, const Worker& w2) {
                               std::unique_lock<std::mutex> l1(w1.mutex_);
                               std::unique_lock<std::mutex> l2(w2.mutex_);
                               return w1.task_.size() < w2.task_.size();
                             });
  it->add(f);
}

ThreadPool::Worker::Worker()
    : running_(true), thread_([this]() {
        while (true) {
          std::unique_lock<std::mutex> lock(mutex_);
          condition_.wait(lock, [=]() { return !running_ || !task_.empty(); });
          if (!running_) break;
          while (!task_.empty()) {
            auto f = task_.back();
            task_.pop_back();
            lock.unlock();
            f();
            lock.lock();
          }
        }
      }) {}

ThreadPool::Worker::~Worker() {
  running_ = false;
  condition_.notify_one();
  thread_.join();
}

void ThreadPool::Worker::add(std::function<void()> f) {
  std::unique_lock<std::mutex> lock(mutex_);
  task_.push_back(f);
  condition_.notify_one();
}

}  // namespace cloudstorage
