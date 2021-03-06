/*****************************************************************************
 * IThreadPool.h
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
#ifndef ITHREADPOOL_H
#define ITHREADPOOL_H

#include <chrono>
#include <functional>
#include <memory>

#include "IRequest.h"

namespace cloudstorage {

class CLOUDSTORAGE_API IThreadPool {
 public:
  using Pointer = std::unique_ptr<IThreadPool>;
  using Task = GenericCallback<>;

  virtual ~IThreadPool() = default;

  static Pointer create(uint32_t thread_count);

  virtual void schedule(const Task& f,
                        const std::chrono::system_clock::time_point& when =
                            std::chrono::system_clock::now()) = 0;
};

}  // namespace cloudstorage

#endif  // ITHREADPOOL_H
