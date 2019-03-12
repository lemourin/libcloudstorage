/*****************************************************************************
 * Http.h
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
#ifndef HTTP_H
#define HTTP_H

#include "Item.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

struct cloud_http;
struct cloud_http_request;
struct cloud_http_callback;
struct cloud_http_headers;

struct cloud_http_response {
  int http_code;
  struct cloud_http_headers* headers;
};

struct cloud_input;
struct cloud_output;

struct cloud_http_operations {
  struct cloud_http_request* (*create)(cloud_string* url, cloud_string* method,
                                       int follow_redirect, void* userdata);
  void (*set_header_parameter)(struct cloud_http_request*,
                               cloud_string* parameter, cloud_string* value);
  void (*set_parameter)(struct cloud_http_request*, cloud_string* parameter,
                        cloud_string* value);
  void (*send)(struct cloud_http_request*,
               void (*completed)(const void* param,
                                 struct cloud_http_response*),
               const void* param, struct cloud_input* data,
               struct cloud_output* response, struct cloud_output* error_stream,
               struct cloud_http_callback*);
  void (*release_http_request)(struct cloud_http_request*);
  void (*release)(void* userdata);
};

CLOUDSTORAGE_API struct cloud_http* cloud_http_create_default();
CLOUDSTORAGE_API struct cloud_http* cloud_http_create(
    struct cloud_http_operations*, void* userdata);
CLOUDSTORAGE_API void cloud_http_release(struct cloud_http*);

CLOUDSTORAGE_API struct cloud_http_headers* cloud_http_headers_create();
CLOUDSTORAGE_API void cloud_http_headers_put(struct cloud_http_headers*,
                                             cloud_string* key,
                                             cloud_string* value);
CLOUDSTORAGE_API void cloud_http_headers_release(struct cloud_http_headers*);

CLOUDSTORAGE_API size_t cloud_input_read(struct cloud_input*, uint8_t* data,
                                         size_t maxbytes);
CLOUDSTORAGE_API size_t cloud_input_size(struct cloud_input*);
CLOUDSTORAGE_API int cloud_output_write(struct cloud_output*, uint8_t* data,
                                        size_t size);

CLOUDSTORAGE_API int cloud_http_callback_is_success(
    struct cloud_http_callback* callback, int code,
    const struct cloud_http_headers* headers);
CLOUDSTORAGE_API int cloud_http_callback_abort(struct cloud_http_callback*);
CLOUDSTORAGE_API int cloud_http_callback_pause(struct cloud_http_callback*);
CLOUDSTORAGE_API void cloud_http_callback_progress_download(
    struct cloud_http_callback*, uint64_t total, uint64_t now);
CLOUDSTORAGE_API void cloud_http_callback_progress_upload(
    struct cloud_http_callback*, uint64_t total, uint64_t now);

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // HTTP_H
