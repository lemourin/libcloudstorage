/*****************************************************************************
 * Request.h
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
#ifndef REQUEST_H
#define REQUEST_H

#include <stdbool.h>
#include <stdint.h>

#include "Item.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

struct cloud_range {
  uint64_t start;
  uint64_t size;
};

#define RANGE_BEGIN 0ull
#define RANGE_FULL -1ull

#define FULL_RANGE \
  { .start = RANGE_BEGIN, .size = RANGE_FULL }

struct cloud_either;
struct cloud_either_token;
struct cloud_either_string;
struct cloud_either_item_list;
struct cloud_either_item;
struct cloud_either_void;
struct cloud_either_page_data;
struct cloud_either_general_data;

struct cloud_error;
struct cloud_token;
struct cloud_item_list;
struct cloud_page_data;
struct cloud_general_data;

struct cloud_request;
struct cloud_request_exchange_code;
struct cloud_request_get_item_url;
struct cloud_request_list_directory;
struct cloud_request_get_item;
struct cloud_request_download_file;
struct cloud_request_upload_file;
struct cloud_request_get_item_data;
struct cloud_request_get_thumbnail;
struct cloud_request_delete_item;
struct cloud_request_create_directory;
struct cloud_request_move_item;
struct cloud_request_rename_item;
struct cloud_request_list_directory_page;
struct cloud_request_get_general_data;

typedef void (*cloud_request_exchange_code_callback)(struct cloud_either_token*,
                                                     void*);
typedef void (*cloud_request_get_item_url_callback)(struct cloud_either_string*,
                                                    void*);
struct cloud_request_list_directory_callback {
  void (*received_item)(struct cloud_item*, void*);
  void (*done)(struct cloud_either_item_list*, void*);
};
typedef void (*cloud_request_get_item_callback)(struct cloud_either_item*,
                                                void*);
typedef void (*cloud_request_get_item_data_callback)(struct cloud_either_item*,
                                                     void*);
typedef void (*cloud_request_delete_item_callback)(struct cloud_either_void*,
                                                   void*);
typedef void (*cloud_request_create_directory_callback)(
    struct cloud_either_item*, void*);
typedef void (*cloud_request_move_item_callback)(struct cloud_either_item*,
                                                 void*);
typedef void (*cloud_request_rename_item_callback)(struct cloud_either_item*,
                                                   void*);
typedef void (*cloud_request_list_directory_simple_callback)(
    struct cloud_either_item_list*, void*);
typedef void (*cloud_request_list_directory_page_callback)(
    struct cloud_either_page_data*, void*);
typedef void (*cloud_request_get_general_data_callback)(
    struct cloud_either_general_data*, void*);
struct cloud_request_upload_file_callback {
  void (*progress)(uint64_t total, uint64_t now, void* data);
  uint64_t (*size)(void* data);
  void (*done)(struct cloud_either_item*, void* data);
  uint32_t (*put_data)(char* buffer, uint32_t maxlength,
                       uint64_t requested_offset, void* data);
};
struct cloud_request_download_file_callback {
  void (*received_data)(const char* data, uint32_t length, void* userdata);
  void (*progress)(uint64_t total, uint64_t now, void* data);
  void (*done)(struct cloud_either_void*, void* data);
};

CLOUDSTORAGE_API void cloud_request_finish(struct cloud_request*);
CLOUDSTORAGE_API void cloud_request_release(struct cloud_request*);

CLOUDSTORAGE_API cloud_string* cloud_error_description(struct cloud_error*);
CLOUDSTORAGE_API int cloud_error_code(struct cloud_error*);
CLOUDSTORAGE_API void cloud_error_release(struct cloud_error*);

CLOUDSTORAGE_API cloud_string* cloud_token_token(struct cloud_token*);
CLOUDSTORAGE_API cloud_string* cloud_token_access_token(struct cloud_token*);
CLOUDSTORAGE_API void cloud_token_release(struct cloud_token*);

CLOUDSTORAGE_API int cloud_item_list_length(const struct cloud_item_list*);
CLOUDSTORAGE_API struct cloud_item* cloud_item_list_get(
    const struct cloud_item_list*, int index);
CLOUDSTORAGE_API void cloud_item_list_release(struct cloud_item_list*);

CLOUDSTORAGE_API cloud_string* cloud_page_next_token(struct cloud_page_data*);
CLOUDSTORAGE_API struct cloud_item_list* cloud_page_item_list(
    struct cloud_page_data*);
CLOUDSTORAGE_API void cloud_page_data_release(struct cloud_page_data*);

CLOUDSTORAGE_API cloud_string* cloud_general_data_username(
    struct cloud_general_data*);
CLOUDSTORAGE_API uint64_t
cloud_general_data_space_total(struct cloud_general_data*);
CLOUDSTORAGE_API uint64_t
cloud_general_data_space_used(struct cloud_general_data*);
CLOUDSTORAGE_API void cloud_general_data_release(struct cloud_general_data*);

CLOUDSTORAGE_API struct cloud_either_item_list*
cloud_request_list_directory_result(struct cloud_request_list_directory*);
CLOUDSTORAGE_API struct cloud_either_token* cloud_request_exchange_code_result(
    struct cloud_request_exchange_code*);
CLOUDSTORAGE_API struct cloud_either_string* cloud_request_get_item_url_result(
    struct cloud_request_get_item_url*);
CLOUDSTORAGE_API struct cloud_either_item* cloud_request_get_item_result(
    struct cloud_request_get_item*);
CLOUDSTORAGE_API struct cloud_either_item* cloud_request_get_item_data_result(
    struct cloud_request_get_item_data*);
CLOUDSTORAGE_API struct cloud_either_void* cloud_request_delete_item_result(
    struct cloud_request_delete_item*);
CLOUDSTORAGE_API struct cloud_either_item*
cloud_request_create_directory_result(struct cloud_request_create_directory*);
CLOUDSTORAGE_API struct cloud_either_item* cloud_request_rename_item_result(
    struct cloud_request_rename_item*);
CLOUDSTORAGE_API struct cloud_either_item* cloud_request_move_item_result(
    struct cloud_request_move_item*);
CLOUDSTORAGE_API struct cloud_either_page_data*
cloud_request_list_directory_page_result(
    struct cloud_request_list_directory_page*);
CLOUDSTORAGE_API struct cloud_either_general_data*
cloud_request_get_general_data_result(struct cloud_request_get_general_data*);
CLOUDSTORAGE_API struct cloud_either_item* cloud_request_upload_file_result(
    struct cloud_request_upload_file*);
CLOUDSTORAGE_API struct cloud_either_void* cloud_request_download_file_result(
    struct cloud_request_download_file*);

CLOUDSTORAGE_API struct cloud_error* cloud_either_string_error(
    struct cloud_either_string*);
CLOUDSTORAGE_API cloud_string* cloud_either_string_result(
    struct cloud_either_string*);
CLOUDSTORAGE_API void cloud_either_string_release(struct cloud_either_string*);

CLOUDSTORAGE_API struct cloud_error* cloud_either_void_error(
    struct cloud_either_void*);
CLOUDSTORAGE_API void cloud_either_void_release(struct cloud_either_void*);

CLOUDSTORAGE_API struct cloud_error* cloud_either_token_error(
    struct cloud_either_token*);
CLOUDSTORAGE_API struct cloud_token* cloud_either_token_result(
    struct cloud_either_token*);
CLOUDSTORAGE_API void cloud_either_token_release(struct cloud_either_token*);

CLOUDSTORAGE_API struct cloud_error* cloud_either_item_list_error(
    struct cloud_either_item_list*);
CLOUDSTORAGE_API struct cloud_item_list* cloud_either_item_list_result(
    struct cloud_either_item_list*);
CLOUDSTORAGE_API void cloud_either_item_list_release(
    struct cloud_either_item_list*);

CLOUDSTORAGE_API struct cloud_error* cloud_either_item_error(
    struct cloud_either_item*);
CLOUDSTORAGE_API struct cloud_item* cloud_either_item_result(
    struct cloud_either_item*);
CLOUDSTORAGE_API void cloud_either_item_release(struct cloud_either_item*);

CLOUDSTORAGE_API struct cloud_error* cloud_either_page_data_error(
    struct cloud_either_page_data*);
CLOUDSTORAGE_API struct cloud_page_data* cloud_either_page_data_result(
    struct cloud_either_page_data*);
CLOUDSTORAGE_API void cloud_either_page_data_release(
    struct cloud_either_page_data*);

CLOUDSTORAGE_API struct cloud_error* cloud_either_general_data_error(
    struct cloud_either_general_data*);
CLOUDSTORAGE_API struct cloud_general_data* cloud_either_general_data_result(
    struct cloud_either_general_data*);
CLOUDSTORAGE_API void cloud_either_general_data_release(
    struct cloud_either_general_data*);

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // REQUEST_H
