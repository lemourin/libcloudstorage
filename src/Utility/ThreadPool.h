/*****************************************************************************
 * ThreadPool.h
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
#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>
#include "IThreadPool.h"

namespace cloudstorage {

class ThreadPool : public IThreadPool {
 public:
  ThreadPool(uint32_t thread_count);

  void schedule(const Task& f) override;

 private:
  struct Worker {
    Worker();
    ~Worker();

    void add(std::function<void()> f);

    mutable std::mutex mutex_;
    bool running_;
    std::vector<std::function<void()>> task_;
    std::condition_variable condition_;
    std::thread thread_;
  };
  std::vector<Worker> worker_;
};

}  // namespace cloudstorage

#endif  // THREADPOOL_H
