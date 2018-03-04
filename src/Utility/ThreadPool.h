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
#include <queue>
#include <thread>
#include <vector>
#include "IThreadPool.h"

namespace cloudstorage {

class ThreadPool : public IThreadPool {
 public:
  ThreadPool(uint32_t thread_count);
  ~ThreadPool();
  void schedule(const Task &f) override;

 private:
  std::mutex mutex_;
  std::condition_variable worker_cv_;
  std::queue<Task> tasks_;
  std::vector<std::thread> workers_;
  bool destroyed_;
};

}  // namespace cloudstorage

#endif  // THREADPOOL_H
