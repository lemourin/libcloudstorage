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

#include <libffmpegthumbnailer/videothumbnailer.h>

#include "IHttp.h"

namespace cloudstorage {

EitherError<std::vector<char>> FFmpegThumbnailer::generateThumbnail(
    IItem::Pointer item) {
  if ((item->type() != IItem::FileType::Image &&
       item->type() != IItem::FileType::Video) ||
      item->url().empty()) {
    Error e{IHttpRequest::Failure,
            "can generate thumbnails only for images and videos"};
    return e;
  }
  std::vector<uint8_t> buffer;
  ffmpegthumbnailer::VideoThumbnailer thumbnailer;
  try {
    thumbnailer.generateThumbnail(item->url(), ThumbnailerImageType::Png,
                                  buffer);
    auto ptr = reinterpret_cast<const char*>(buffer.data());
    return std::vector<char>(ptr, ptr + buffer.size());
  } catch (const std::exception& e) {
    return Error{IHttpRequest::Failure, e.what()};
  }
}

}  // namespace cloudstorage
