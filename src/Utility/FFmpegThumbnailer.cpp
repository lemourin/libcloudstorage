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
          it->second.future_.wait();
          thumbnail_threads_.erase(it);
        }
        finished_.pop_back();
      }
      if (destroyed_) break;
    }
  });
}

FFmpegThumbnailer::~FFmpegThumbnailer() {
  for (const auto& t : thumbnail_threads_) t.second.future_.wait();
  destroyed_ = true;
  condition_.notify_one();
  cleanup_thread_.wait();
}

IRequest<std::vector<char>>::Pointer FFmpegThumbnailer::generateThumbnail(
    std::shared_ptr<ICloudProvider> p, IItem::Pointer item, Callback callback,
    ErrorCallback error_callback) {
  auto r = make_unique<Request<std::vector<char>>>(
      std::static_pointer_cast<CloudProvider>(p));
  r->set_resolver([this, item, callback, error_callback](
                      Request<std::vector<char>>* r) -> std::vector<char> {
    if ((item->type() != IItem::FileType::Image &&
         item->type() != IItem::FileType::Video) ||
        item->url().empty()) {
      error_callback("Can generate thumbnails only for images and videos.");
      callback({});
      return {};
    }
    std::shared_future<std::vector<char>> future;
    std::shared_ptr<std::condition_variable> done;
    std::shared_ptr<bool> finished;
    std::shared_ptr<std::string> error_description;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      auto it = thumbnail_threads_.find(item->url());
      std::string url = item->url();
      if (it == std::end(thumbnail_threads_)) {
        done = std::make_shared<std::condition_variable>();
        finished = std::make_shared<bool>(false);
        error_description = std::make_shared<std::string>("");
        future = std::async(std::launch::async, [this, url, done, finished,
                                                 error_description]() {
          std::vector<uint8_t> buffer;
          ffmpegthumbnailer::VideoThumbnailer thumbnailer;
          try {
            thumbnailer.generateThumbnail(url, ThumbnailerImageType::Png,
                                          buffer);
          } catch (const std::exception& e) {
            std::lock_guard<std::mutex> lock(mutex_);
            *error_description = e.what();
          }
          {
            std::lock_guard<std::mutex> lock(mutex_);
            finished_.push_back(url);
            *finished = true;
          }
          done->notify_all();
          condition_.notify_one();
          auto ptr = reinterpret_cast<const char*>(buffer.data());
          return std::vector<char>(ptr, ptr + buffer.size());
        });
        thumbnail_threads_[url] = {future, done, finished, error_description};
      } else {
        future = it->second.future_;
        done = it->second.done_;
        finished = it->second.finished_;
        error_description = it->second.error_description_;
      }
    }
    r->set_cancel_callback([done]() { done->notify_all(); });
    {
      std::unique_lock<std::mutex> lock(mutex_);
      done->wait(lock, [=]() { return r->is_cancelled() || *finished; });
    }
    if (r->is_cancelled()) {
      callback({});
      return {};
    }
    auto result = future.get();
    if (!error_description->empty()) error_callback(*error_description);
    callback(result);
    return result;
  });
  return std::move(r);
}

}  // namespace cloudstorage
