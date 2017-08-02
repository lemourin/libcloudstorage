/*****************************************************************************
 * Utility.h : prototypes for utility functions
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

#ifndef UTILITY_H
#define UTILITY_H

#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>

namespace cloudstorage {

namespace util {
template <typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

struct range {
  int64_t start, size;
};

std::string remove_whitespace(const std::string& str);
range parse_range(const std::string& str);
std::string address(const std::string& url, uint16_t port);
std::string to_mime_type(const std::string& extension);

class Url {
 public:
  static std::string unescape(const std::string&);
  static std::string escape(const std::string&);
  static std::string escapeHeader(const std::string&);
};
}  // namespace util

class Semaphore {
 public:
  Semaphore() : count_() {}

  void notify() {
    std::unique_lock<std::mutex> lock(mutex_);
    count_++;
    condition_.notify_one();
  }

  void wait() {
    std::unique_lock<std::mutex> lock(mutex_);
    while (count_ == 0) condition_.wait(lock);
    count_--;
  }

 private:
  std::mutex mutex_;
  std::condition_variable condition_;
  uint32_t count_;
};

}  // namespace cloudstorage

#endif  // UTILITY_H
