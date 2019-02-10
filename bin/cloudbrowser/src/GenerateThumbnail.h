/*****************************************************************************
 * GenerateThumbnail.h
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
#ifndef GENERATE_THUMBNAIL_H
#define GENERATE_THUMBNAIL_H

#ifdef WITH_THUMBNAILER

#include "IRequest.h"

namespace cloudstorage {

EitherError<std::string> generate_thumbnail(
    const std::string& url,
    std::function<bool(std::chrono::system_clock::time_point)> interrupt);

EitherError<std::string> generate_thumbnail(
    const std::string& url, int64_t timestamp,
    std::function<bool(std::chrono::system_clock::time_point)> interrupt);

}  // namespace cloudstorage

#endif  // WITH_THUMBNAILER
#endif  // GENERATE_THUMBNAIL_H
