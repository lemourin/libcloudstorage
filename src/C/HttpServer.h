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

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

struct cloud_http_server_factory;
struct cloud_http_server_response;
struct cloud_http_server_request;
struct cloud_http_server_callback;
struct cloud_http_server;

typedef void (*cloud_http_server_release)(void*);
typedef struct cloud_http_server* (*cloud_http_server_create_callback)(
    void* data, struct cloud_http_server_callback*, cloud_string* session,
    int type);

CLOUDSTORAGE_API struct cloud_http_server_factory* cloud_http_server_factory_default_create();
CLOUDSTORAGE_API struct cloud_http_server_factory* cloud_http_server_factory_create(
    cloud_http_server_create_callback, void* data);
CLOUDSTORAGE_API void cloud_http_server_factory_release(struct cloud_http_server_factory*);

CLOUDSTORAGE_API struct cloud_http_server* cloud_http_server_create(
    struct cloud_http_server_callback*, cloud_http_server_release, void* data);
CLOUDSTORAGE_API struct cloud_http_server_response* cloud_http_server_handle(
    struct cloud_http_server*, struct cloud_http_server_request*);

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // HTTP_SERVER_H
