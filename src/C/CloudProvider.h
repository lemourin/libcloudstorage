/*****************************************************************************
 * CloudProvider.h
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
#ifndef CLOUD_PROVIDER_H
#define CLOUD_PROVIDER_H

#include <stdint.h>

#include "Crypto.h"
#include "Http.h"
#include "HttpServer.h"
#include "Item.h"
#include "Request.h"
#include "ThreadPool.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

enum cloud_permission {
  cloud_read_meta_data,  // list files
  cloud_read,            // read files
  cloud_read_write       // modify files
};

enum cloud_status { cloud_wait_for_authorization_code, cloud_none };

struct cloud_provider;
struct cloud_hints;
struct cloud_provider_init_data;

struct cloud_provider_auth_callback {
  enum cloud_status (*user_consent_required)(const struct cloud_provider*,
                                             void* data);
  void (*done)(struct cloud_either_void*, void* data);
};

struct cloud_provider_init_data_args {
  cloud_string* token;
  enum cloud_permission permission;
  struct cloud_provider_auth_callback auth_callback;
  void* auth_data;
  struct cloud_crypto* crypto;
  struct cloud_http* http;
  struct cloud_http_server_factory* http_server;
  struct cloud_thread_pool* thread_pool;
  struct cloud_hints* hints;
};

CLOUDSTORAGE_API struct cloud_hints* cloud_hints_create();
CLOUDSTORAGE_API void cloud_hints_add(struct cloud_hints* hints,
                                      cloud_string* key, cloud_string* value);
CLOUDSTORAGE_API void cloud_hints_release(struct cloud_hints*);

CLOUDSTORAGE_API struct cloud_provider_init_data*
cloud_provider_init_data_create(const struct cloud_provider_init_data_args*);
CLOUDSTORAGE_API void cloud_provider_init_data_release(
    struct cloud_provider_init_data*);

CLOUDSTORAGE_API void cloud_provider_release(struct cloud_provider*);
CLOUDSTORAGE_API uint32_t
cloud_provider_supported_operations(const struct cloud_provider*);
CLOUDSTORAGE_API cloud_string* cloud_provider_token(
    const struct cloud_provider*);
CLOUDSTORAGE_API struct cloud_hints* cloud_provider_hints(
    const struct cloud_provider*);
CLOUDSTORAGE_API cloud_string* cloud_provider_name(
    const struct cloud_provider*);
CLOUDSTORAGE_API cloud_string* cloud_provider_endpoint(
    const struct cloud_provider*);
CLOUDSTORAGE_API cloud_string* cloud_provider_authorize_library_url(
    const struct cloud_provider*);
CLOUDSTORAGE_API struct cloud_item* cloud_provider_root_directory(
    const struct cloud_provider*);

CLOUDSTORAGE_API struct cloud_request_exchange_code*
cloud_provider_exchange_code(struct cloud_provider*, const char* code,
                             cloud_request_exchange_code_callback, void* data);
CLOUDSTORAGE_API struct cloud_request_get_item_url* cloud_provider_get_item_url(
    struct cloud_provider*, struct cloud_item*,
    cloud_request_get_item_url_callback, void* data);
CLOUDSTORAGE_API struct cloud_request_list_directory*
cloud_provider_list_directory(struct cloud_provider*, struct cloud_item*,
                              struct cloud_request_list_directory_callback*,
                              void* data);
CLOUDSTORAGE_API struct cloud_request_get_item* cloud_provider_get_item(
    struct cloud_provider*, cloud_string* path, cloud_request_get_item_callback,
    void* data);
CLOUDSTORAGE_API struct cloud_request_get_item_data*
cloud_provider_get_item_data(struct cloud_provider*, cloud_string* id,
                             cloud_request_get_item_data_callback, void* data);
CLOUDSTORAGE_API struct cloud_request_delete_item* cloud_provider_delete_item(
    struct cloud_provider*, struct cloud_item* item,
    cloud_request_delete_item_callback, void* data);
CLOUDSTORAGE_API struct cloud_request_create_directory*
cloud_provider_create_directory(struct cloud_provider*,
                                struct cloud_item* parent, cloud_string* name,
                                cloud_request_create_directory_callback,
                                void* data);
CLOUDSTORAGE_API struct cloud_request_move_item* cloud_provider_move_item(
    struct cloud_provider*, struct cloud_item* item,
    struct cloud_item* new_parent, cloud_request_move_item_callback,
    void* data);
CLOUDSTORAGE_API struct cloud_request_rename_item* cloud_provider_rename_item(
    struct cloud_provider*, struct cloud_item* item, cloud_string* new_name,
    cloud_request_rename_item_callback, void* data);
CLOUDSTORAGE_API struct cloud_request_list_directory*
cloud_provider_list_directory_simple(
    struct cloud_provider*, struct cloud_item* item,
    cloud_request_list_directory_simple_callback, void* data);
CLOUDSTORAGE_API struct cloud_request_list_directory_page*
cloud_provider_list_directory_page(struct cloud_provider*,
                                   struct cloud_item* item, cloud_string* token,
                                   cloud_request_list_directory_page_callback,
                                   void* data);
CLOUDSTORAGE_API struct cloud_request_get_general_data*
cloud_provider_get_general_data(struct cloud_provider*,
                                cloud_request_get_general_data_callback, void*);
CLOUDSTORAGE_API struct cloud_request_get_item_url*
cloud_provider_get_file_daemon_url(struct cloud_provider*,
                                   struct cloud_item* item,
                                   cloud_request_get_item_url_callback, void*);
CLOUDSTORAGE_API struct cloud_request_upload_file* cloud_provider_upload_file(
    struct cloud_provider*, struct cloud_item* parent, cloud_string* filename,
    struct cloud_request_upload_file_callback*, void* data);
CLOUDSTORAGE_API struct cloud_request_download_file*
cloud_provider_download_file(struct cloud_provider*, struct cloud_item* item,
                             struct cloud_range range,
                             struct cloud_request_download_file_callback*,
                             void*);

CLOUDSTORAGE_API cloud_string* cloud_provider_serialize_session(
    cloud_string* token, const struct cloud_hints* hints);
CLOUDSTORAGE_API int cloud_provider_deserialize_session(
    cloud_string* data, cloud_string** token, struct cloud_hints** hints);

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // CLOUD_PROVIDER_H
