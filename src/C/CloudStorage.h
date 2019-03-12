/*****************************************************************************
 * CloudStorage.h
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
#ifndef CLOUDSTORAGE_H
#define CLOUDSTORAGE_H

#include <stddef.h>

#include "CloudProvider.h"
#include "Item.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

struct cloud_storage;
struct cloud_string_list;

CLOUDSTORAGE_API struct cloud_storage* cloud_storage_create();
CLOUDSTORAGE_API void cloud_storage_release(struct cloud_storage*);
CLOUDSTORAGE_API struct cloud_string_list* cloud_storage_provider_list(
    struct cloud_storage*);
CLOUDSTORAGE_API struct cloud_provider* cloud_storage_provider(
    struct cloud_storage*, cloud_string* name,
    struct cloud_provider_init_data*);

CLOUDSTORAGE_API void cloud_string_list_release(struct cloud_string_list*);
CLOUDSTORAGE_API size_t cloud_string_list_length(struct cloud_string_list*);
CLOUDSTORAGE_API cloud_string* cloud_string_list_get(struct cloud_string_list*,
                                                     size_t idx);

CLOUDSTORAGE_API cloud_string* cloud_string_create(const char*);
CLOUDSTORAGE_API void cloud_string_release(cloud_string*);

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // CLOUDSTORAGE_H
