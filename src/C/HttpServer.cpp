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
  HttpServer(cloud_http_server *server, cloud_http_server_operations ops,
             ICallback::Pointer callback)
      : http_server_(server),
        operations_(ops),
        callback_(std::move(callback)) {}

  ~HttpServer() override { operations_.release_http_server(http_server_); }

  ICallback::Pointer callback() const override { return callback_; }

 private:
  cloud_http_server *http_server_;
  cloud_http_server_operations operations_;
  ICallback::Pointer callback_;
};
}  // namespace

cloud_http_server_factory *cloud_http_server_factory_create(
    cloud_http_server_operations *d, void *data) {
  class HttpServerFactory : public IHttpServerFactory {
   public:
    HttpServerFactory(cloud_http_server_operations ops, void *data)
        : operations_(ops), data_(data) {}

    ~HttpServerFactory() override { operations_.release(data_); }

    IHttpServer::Pointer create(IHttpServer::ICallback::Pointer callback,
                                const std::string &session_id,
                                IHttpServer::Type type) override {
      auto cserver = operations_.create(
          reinterpret_cast<cloud_http_server_callback *>(callback.get()),
          session_id.c_str(), static_cast<int>(type), data_);
      return util::make_unique<HttpServer>(cserver, operations_, callback);
    }

   private:
    cloud_http_server_operations operations_;
    void *data_;
  };
  return reinterpret_cast<cloud_http_server_factory *>(
      util::make_unique<HttpServerFactory>(*d, data).release());
}

void cloud_http_server_factory_release(cloud_http_server_factory *d) {
  delete reinterpret_cast<IHttpServerFactory *>(d);
}

cloud_http_server_factory *cloud_http_server_factory_default_create() {
  return reinterpret_cast<cloud_http_server_factory *>(
      IHttpServerFactory::create().release());
}

cloud_http_server_response *cloud_http_server_handle(
    cloud_http_server *d, cloud_http_server_request *request) {
  auto server = reinterpret_cast<HttpServer *>(d);
  return reinterpret_cast<cloud_http_server_response *>(
      server->callback()
          ->handle(*reinterpret_cast<IHttpServer::IRequest *>(request))
          .release());
}

cloud_http_server_request *cloud_http_server_request_create(
    cloud_http_server_request_operations *ops, void *userdata) {
  struct HttpResponse : public IHttpServer::IResponse {
    HttpResponse(cloud_http_server_request_operations ops,
                 cloud_http_server_response *response,
                 IResponse::ICallback::Pointer callback)
        : operations_(ops),
          response_(response),
          callback_(std::move(callback)) {}

    ~HttpResponse() override { operations_.release_response(response_); }

    void resume() override { operations_.resume(response_); }

    void completed(CompletedCallback cb) override {
      operations_.complete(
          response_,
          [](const void *param) {
            auto cb = reinterpret_cast<const CompletedCallback *>(param);
            (*cb)();
            delete cb;
          },
          new CompletedCallback(cb));
    }

    cloud_http_server_request_operations operations_;
    cloud_http_server_response *response_;
    IResponse::ICallback::Pointer callback_;
  };

  struct HttpRequest : public IHttpServer::IRequest {
    HttpRequest(cloud_http_server_request_operations ops, void *userdata)
        : operations_(ops), userdata_(userdata) {}

    ~HttpRequest() override { operations_.release(userdata_); }

    const char *get(const std::string &name) const override {
      cloud_string *str = operations_.get(name.c_str(), userdata_);
      if (!str) return nullptr;
      storage_ = str;
      free(const_cast<char *>(str));
      return storage_.c_str();
    }
    const char *header(const std::string &name) const override {
      cloud_string *str = operations_.header(name.c_str(), userdata_);
      if (!str) return nullptr;
      storage_ = str;
      free(const_cast<char *>(str));
      return storage_.c_str();
    }
    std::string method() const override {
      cloud_string *str = operations_.method(userdata_);
      std::string result = str;
      free(const_cast<char *>(str));
      return result;
    }
    std::string url() const override {
      cloud_string *str = operations_.url(userdata_);
      std::string result = str;
      free(const_cast<char *>(str));
      return result;
    }

    IHttpServer::IResponse::Pointer response(
        int code, const IHttpServer::IResponse::Headers &headers, int64_t size,
        IHttpServer::IResponse::ICallback::Pointer callback) const override {
      cloud_http_server_response *r = operations_.response(
          code,
          reinterpret_cast<const cloud_http_server_response_headers *>(
              &headers),
          size,
          reinterpret_cast<cloud_http_server_response_callback *>(
              callback.get()));
      return util::make_unique<HttpResponse>(operations_, r,
                                             std::move(callback));
    }

    cloud_http_server_request_operations operations_;
    void *userdata_;
    mutable std::string storage_;
  };
  return reinterpret_cast<cloud_http_server_request *>(
      new HttpRequest(*ops, userdata));
}

int cloud_http_server_response_callback_put_data(
    cloud_http_server_response_callback *callback, uint8_t *data, size_t size) {
  return reinterpret_cast<IHttpServer::IResponse::ICallback *>(callback)
      ->putData(reinterpret_cast<char *>(data), size);
}

void cloud_http_server_response_headers_iterate(
    cloud_http_server_response_headers *d,
    void (*callback)(cloud_string *, cloud_string *, void *), void *userdata) {
  const auto &headers =
      *reinterpret_cast<const IHttpServer::IResponse::Headers *>(d);
  for (const auto &p : headers) {
    callback(p.first.c_str(), p.second.c_str(), userdata);
  }
}
