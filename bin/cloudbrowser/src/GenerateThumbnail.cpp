/*****************************************************************************
 * GenerateThumbnail.cpp
 *
 *****************************************************************************
 * Copyright (C) 2016 VideoLAN
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

#ifdef WITH_THUMBNAILER

#include "GenerateThumbnail.h"

#include <libffmpegthumbnailer/videothumbnailer.h>
#include <cstring>

extern "C" {
#include <libavformat/avformat.h>
}

#include <mutex>
#include "IHttp.h"
#include "IRequest.h"

namespace cloudstorage {

const auto SIZE = 128;

EitherError<std::string> generate_thumbnail(const std::string& url,
                                            std::function<bool()> interrupt) {
  std::vector<uint8_t> buffer;
  ffmpegthumbnailer::VideoThumbnailer thumbnailer(128, true, false, 8, false);
  std::string effective_url = url;
#ifdef _WIN32
  const char* file = "file:///";
#else
  const char* file = "file://";
#endif
  const auto length = strlen(file);
  if (url.substr(0, length) == file) effective_url = url.substr(length);
  AVFormatContext* context = avformat_alloc_context();
  context->interrupt_callback.opaque = &interrupt;
  context->interrupt_callback.callback = [](void* t) -> int {
    return (*reinterpret_cast<std::function<bool()>*>(t))();
  };
  av_register_all();
  avformat_network_init();
  Error error{
      avformat_open_input(&context, effective_url.c_str(), nullptr, nullptr),
      ""};
  if (error.code_ < 0) {
    char buffer[SIZE + 1] = {};
    av_strerror(error.code_, buffer, SIZE);
    error = {IHttpRequest::Failure, buffer};
  } else {
    try {
      thumbnailer.generateThumbnail(effective_url, ThumbnailerImageType::Png,
                                    buffer, context);
    } catch (const std::exception& e) {
      error = {IHttpRequest::Failure, e.what()};
    }
  }
  avformat_network_deinit();
  avformat_free_context(context);
  if (error.code_ == 0 && !buffer.empty()) {
    auto ptr = reinterpret_cast<const char*>(buffer.data());
    return std::string(ptr, ptr + buffer.size());
  } else {
    if (error.code_ != 0)
      return error;
    else
      return Error{IHttpRequest::Failure, "thumbnailer interrupted"};
  }
}

void abort_thumbnailer() {}

}  // namespace cloudstorage

#endif  // WITH_THUMBNAILER
