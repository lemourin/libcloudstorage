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

#include "IHttp.h"
#include "IRequest.h"

namespace cloudstorage {

EitherError<std::string> generate_thumbnail(const std::string& url) {
  try {
    std::vector<uint8_t> buffer;
    ffmpegthumbnailer::VideoThumbnailer thumbnailer;
    thumbnailer.generateThumbnail(url, ThumbnailerImageType::Png, buffer);
    auto ptr = reinterpret_cast<const char*>(buffer.data());
    return std::string(ptr, ptr + buffer.size());
  } catch (const std::exception& e) {
    return Error{IHttpRequest::Bad, e.what()};
  }
}

}  // namespace cloudstorage

#endif  // WITH_THUMBNAILER
