/*****************************************************************************
 * ThreadPool.h
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
#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <stdint.h>
#include "Item.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

struct cloud_thread_pool;

struct cloud_thread_pool_operations {
  void (*schedule)(void (*)(const void* param), const void* param,
                   void* userdata);
  void (*release)(void* userdata);
};

CLOUDSTORAGE_API struct cloud_thread_pool* cloud_thread_pool_create_default(
    uint32_t thread_count);
CLOUDSTORAGE_API struct cloud_thread_pool* cloud_thread_pool_create(
    struct cloud_thread_pool_operations*, void* userdata);
CLOUDSTORAGE_API void cloud_thread_pool_release(struct cloud_thread_pool*);

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // THREAD_POOL_H
