/*****************************************************************************
 * Item.h
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
#ifndef ITEM_H
#define ITEM_H

#include <time.h>

#ifndef CLOUDSTORAGE_API
#ifdef _MSC_VER
#ifdef CLOUDSTORAGE_LIBRARY
#define CLOUDSTORAGE_API __declspec(dllexport)
#else
#define CLOUDSTORAGE_API __declspec(dllimport)
#endif
#else
#define CLOUDSTORAGE_API
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

struct cloud_item;
typedef const char cloud_string;

enum cloud_item_type {
  cloud_item_directory,
  cloud_item_video,
  cloud_item_audio,
  cloud_item_image,
  cloud_item_unknown
};

CLOUDSTORAGE_API void cloud_item_release(struct cloud_item*);
CLOUDSTORAGE_API cloud_string* cloud_item_filename(const struct cloud_item*);
CLOUDSTORAGE_API cloud_string* cloud_item_id(const struct cloud_item*);
CLOUDSTORAGE_API cloud_string* cloud_item_extension(const struct cloud_item*);
CLOUDSTORAGE_API cloud_string* cloud_item_to_string(const struct cloud_item*);
CLOUDSTORAGE_API time_t cloud_item_timestamp(const struct cloud_item*);
CLOUDSTORAGE_API size_t cloud_item_size(const struct cloud_item*);
CLOUDSTORAGE_API enum cloud_item_type cloud_item_type(const struct cloud_item*);
CLOUDSTORAGE_API int cloud_item_is_hidden(const struct cloud_item*);
CLOUDSTORAGE_API struct cloud_item* cloud_item_from_string(cloud_string*);

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // ITEM_H
