/*****************************************************************************
 * FFmpegThumbnailer.h : FFmpegThumbnailer interface
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

#ifndef FFMPEGTHUMBNAILER_H
#define FFMPEGTHUMBNAILER_H

#include "IThumbnailer.h"
#include "Request/Request.h"
#include "Utility/Utility.h"

#include <atomic>
#include <thread>

namespace cloudstorage {

class FFmpegThumbnailer : public IThumbnailer {
 public:
  FFmpegThumbnailer();
  ~FFmpegThumbnailer();

  IRequest<std::vector<char>>::Pointer generateThumbnail(
      std::shared_ptr<ICloudProvider> provider, IItem::Pointer item,
      Callback, ErrorCallback) override;

 private:
  struct WorkerData {
    std::shared_future<std::vector<char>> future_;
    std::shared_ptr<std::condition_variable> done_;
    std::shared_ptr<bool> finished_;
    std::shared_ptr<std::string> error_description_;
  };

  std::mutex mutex_;
  std::condition_variable condition_;
  std::unordered_map<std::string, WorkerData> thumbnail_threads_;
  std::vector<std::string> finished_;
  std::future<void> cleanup_thread_;
  std::atomic_bool destroyed_;
};

}  // namespace cloudstorage

#endif  // FFMPEGTHUMBNAILER_H
