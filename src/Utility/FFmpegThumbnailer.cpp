/*****************************************************************************
 * FFmpegThumbnailer.cpp : FFmpegThumbnailer implementation
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

#include "FFmpegThumbnailer.h"

#include "CloudProvider/CloudProvider.h"

#include <libffmpegthumbnailer/videothumbnailer.h>

namespace cloudstorage {

FFmpegThumbnailer::FFmpegThumbnailer() : destroyed_(false) {
  cleanup_thread_ = std::async(std::launch::async, [this]() {
    std::unique_lock<std::mutex> lock(mutex_);
    while (true) {
      condition_.wait(lock,
                      [this]() { return destroyed_ || finished_.size() > 0; });
      while (finished_.size() > 0) {
        auto it = thumbnail_threads_.find(finished_.back());
        if (it != thumbnail_threads_.end()) {
          it->second.wait();
          thumbnail_threads_.erase(it);
        }
        finished_.pop_back();
      }
      if (destroyed_) break;
    }
  });
}

FFmpegThumbnailer::~FFmpegThumbnailer() {
  for (auto t : thumbnail_threads_) t.second.wait();
  destroyed_ = true;
  condition_.notify_one();
  cleanup_thread_.wait();
}

IRequest<std::vector<char>>::Pointer FFmpegThumbnailer::generateThumbnail(
    std::shared_ptr<ICloudProvider> p, IItem::Pointer item,
    std::function<void(const std::vector<char>&)> callback) {
  auto r = make_unique<Request<std::vector<char>>>(
      std::static_pointer_cast<CloudProvider>(p));
  auto semaphore = std::make_shared<Semaphore>();
  r->set_cancel_callback([semaphore]() { semaphore->notify(); });
  r->set_resolver([this, item, callback, semaphore](
                      Request<std::vector<char>>* r) -> std::vector<char> {
    if (item->type() != IItem::FileType::Image &&
        item->type() != IItem::FileType::Video) {
      callback({});
      return {};
    }
    std::shared_future<std::vector<char>> future;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      auto it = thumbnail_threads_.find(item->id());
      if (it == std::end(thumbnail_threads_)) {
        future = std::async(std::launch::async, [this, item, semaphore]() {
          std::vector<uint8_t> buffer;
          ffmpegthumbnailer::VideoThumbnailer thumbnailer;
          thumbnailer.generateThumbnail(item->url(), ThumbnailerImageType::Png,
                                        buffer);
          {
            std::lock_guard<std::mutex> lock(mutex_);
            finished_.push_back(item->id());
          }
          condition_.notify_one();
          semaphore->notify();
          auto ptr = reinterpret_cast<const char*>(buffer.data());
          return std::vector<char>(ptr, ptr + buffer.size());
        });
        thumbnail_threads_[item->id()] = future;
      } else
        future = it->second;
    }
    semaphore->wait();
    if (r->is_cancelled()) {
      callback({});
      return {};
    }
    auto result = future.get();
    callback(result);
    return result;
  });
  return std::move(r);
}

}  // namespace cloudstorage
