/*****************************************************************************
 * HttpServer.h
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
#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "Item.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

#define cloud_http_server_response_callback_suspend 0
#define cloud_http_server_response_callback_abort -1
#define cloud_http_server_response_callback_end -2

struct cloud_http_server_factory;
struct cloud_http_server_response;
struct cloud_http_server_response_callback;
struct cloud_http_server_response_headers;
struct cloud_http_server_callback;
struct cloud_http_server;

struct cloud_http_server_request_operations {
  void (*release)(void* userdata);
  cloud_string* (*get)(cloud_string* name, void* userdata);
  cloud_string* (*header)(cloud_string* name, void* userdata);
  cloud_string* (*method)(void* userdata);
  cloud_string* (*url)(void* userdata);
  struct cloud_http_server_response* (*response)(
      int code, const struct cloud_http_server_response_headers*, int64_t size,
      struct cloud_http_server_response_callback*);

  void (*release_response)(struct cloud_http_server_response*);
  void (*resume)(struct cloud_http_server_response*);
  void (*complete)(struct cloud_http_server_response*,
                   void (*callback)(const void* param), const void* param);
};

struct cloud_http_server_operations {
  void (*release)(void* userdata);
  void (*release_http_server)(struct cloud_http_server*);
  struct cloud_http_server* (*create)(struct cloud_http_server_callback*,
                                      cloud_string* session, int type,
                                      void* userdata);
};

CLOUDSTORAGE_API struct cloud_http_server_factory*
cloud_http_server_factory_default_create();
CLOUDSTORAGE_API struct cloud_http_server_factory*
cloud_http_server_factory_create(struct cloud_http_server_operations*,
                                 void* data);
CLOUDSTORAGE_API void cloud_http_server_factory_release(
    struct cloud_http_server_factory*);

CLOUDSTORAGE_API struct cloud_http_server_request*
cloud_http_server_request_create(struct cloud_http_server_request_operations*,
                                 void* userdata);

CLOUDSTORAGE_API struct cloud_http_server_response* cloud_http_server_handle(
    struct cloud_http_server*, struct cloud_http_server_request*);
CLOUDSTORAGE_API int cloud_http_server_response_callback_put_data(
    struct cloud_http_server_response_callback*, uint8_t* data, size_t size);

CLOUDSTORAGE_API void cloud_http_server_response_headers_iterate(
    struct cloud_http_server_response_headers*,
    void (*callback)(cloud_string* key, cloud_string* value, void* userdata),
    void* userdata);

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // HTTP_SERVER_H
