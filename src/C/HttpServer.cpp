/*****************************************************************************
 * HttpServer.cpp
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
#include "HttpServer.h"

#include "IHttpServer.h"
#include "Utility/Utility.h"

using namespace cloudstorage;

namespace {
class HttpServer : public IHttpServer {
 public:
  HttpServer(cloud_http_server_callback *callback,
             cloud_http_server_release release, void *data)
      : callback_(
            *reinterpret_cast<IHttpServer::ICallback::Pointer *>(callback)),
        release_(release),
        data_(data) {}

  ~HttpServer() { release_(data_); }

  ICallback::Pointer callback() const override { return callback_; }
  void *data() const { return data_; }

 private:
  ICallback::Pointer callback_;
  cloud_http_server_release release_;
  void *data_;
};
}  // namespace

cloud_http_server_factory *cloud_http_server_factory_create(
    cloud_http_server_create_callback callback, void *data) {
  class HttpServerFactory : public IHttpServerFactory {
   public:
    HttpServerFactory(cloud_http_server_create_callback callback, void *data)
        : callback_(callback), data_(data) {}

    IHttpServer::Pointer create(IHttpServer::ICallback::Pointer callback,
                                const std::string &session_id,
                                IHttpServer::Type type) override {
      return std::unique_ptr<IHttpServer>(
          reinterpret_cast<IHttpServer *>(callback_(
              data_, reinterpret_cast<cloud_http_server_callback *>(&callback),
              session_id.c_str(), static_cast<int>(type))));
    }

   private:
    cloud_http_server_create_callback callback_;
    void *data_;
  };
  return reinterpret_cast<cloud_http_server_factory *>(
      new std::unique_ptr<IHttpServerFactory>(
          util::make_unique<HttpServerFactory>(callback, data)));
}

void cloud_http_server_factory_release(cloud_http_server_factory *d) {
  delete reinterpret_cast<std::unique_ptr<IHttpServerFactory> *>(d);
}

cloud_http_server_factory *cloud_http_server_factory_default_create() {
  return reinterpret_cast<cloud_http_server_factory *>(
      new std::unique_ptr<IHttpServerFactory>(IHttpServerFactory::create()));
}

cloud_http_server *cloud_http_server_create(
    cloud_http_server_callback *callback, cloud_http_server_release release,
    void *data) {
  return reinterpret_cast<cloud_http_server *>(
      new HttpServer(callback, release, data));
}

cloud_http_server_response *cloud_http_server_handle(
    cloud_http_server *d, cloud_http_server_request *request) {
  auto server = reinterpret_cast<HttpServer *>(d);
  return reinterpret_cast<cloud_http_server_response *>(
      server->callback()
          ->handle(*reinterpret_cast<IHttpServer::IRequest *>(request))
          .release());
}
