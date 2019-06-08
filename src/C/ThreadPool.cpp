/*****************************************************************************
 * ThreadPool.cpp
 *
 *****************************************************************************
 * Copyright (C) 2016-2018 VideoLAN
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
#include "IThreadPool.h"
#include "Utility/Utility.h"

using namespace cloudstorage;

cloud_thread_pool *cloud_thread_pool_create_default(uint32_t thread_count) {
  return reinterpret_cast<cloud_thread_pool *>(
      IThreadPool::create(thread_count).release());
}

void cloud_thread_pool_release(cloud_thread_pool *thread_pool) {
  delete reinterpret_cast<IThreadPool *>(thread_pool);
}

cloud_thread_pool *cloud_thread_pool_create(cloud_thread_pool_operations *d,
                                            void *userdata) {
  struct ThreadPool : public IThreadPool {
    ThreadPool(cloud_thread_pool_operations ops, void *userdata)
        : operations_(ops), userdata_(userdata) {}

    ~ThreadPool() override { operations_.release(userdata_); }

    void schedule(const Task &f,
                  const std::chrono::system_clock::time_point &when) override {
      operations_.schedule(
          [](const void *d) {
            const Task *task = reinterpret_cast<const Task *>(d);
            (*task)();
            delete task;
          },
          new Task(f), std::chrono::system_clock::to_time_t(when), userdata_);
    }

    cloud_thread_pool_operations operations_;
    void *userdata_;
  };

  return reinterpret_cast<cloud_thread_pool *>(
      util::make_unique<ThreadPool>(*d, userdata).release());
}
