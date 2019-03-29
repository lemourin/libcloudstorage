/*****************************************************************************
 * MicroHttpdServer.cpp : implementation of MicroHttpdServer
 *
 *****************************************************************************
 * Copyright (C) 2016-2016 VideoLAN
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

#ifdef WITH_MICROHTTPD

#include "MicroHttpdServer.h"

#include <microhttpd.h>
#include "Utility.h"

namespace cloudstorage {

const int CHUNK_SIZE = 1024;
const int AUTHORIZATION_PORT = 12345;
const int FILE_PROVIDER_PORT = 12346;

namespace {

struct ConnectionData {
  bool created_ = true;
  IHttpServer::IResponse::Pointer response_;
};

int http_request_callback(void* cls, MHD_Connection* c, const char* url,
                          const char* method, const char* /*version*/,
                          const char* /*upload_data*/, size_t* upload_data_size,
                          void** con_cls) {
  MicroHttpdServer* server = static_cast<MicroHttpdServer*>(cls);
  if (auto d = static_cast<ConnectionData*>(*con_cls)) {
    int ret = MHD_YES;
    if (*upload_data_size == 0) {
      auto response =
          server->callback()->handle(MicroHttpdServer::Request(c, url, method));
      auto p = static_cast<MicroHttpdServer::Response*>(response.get());
      ret = MHD_queue_response(c, p->code(), p->response());
      d->response_ = std::move(response);
    } else {
      *upload_data_size = 0;
    }
    return ret;
  } else {
    *con_cls = new ConnectionData();
    return MHD_YES;
  }
}

void http_request_completed(void*, MHD_Connection*, void** con_cls,
                            MHD_RequestTerminationCode) {
  auto d = static_cast<ConnectionData*>(*con_cls);
  auto p = static_cast<MicroHttpdServer::Response*>(d->response_.get());
  if (p && p->callback()) p->callback()();
  delete d;
}

}  // namespace

IHttpServerFactory::Pointer IHttpServerFactory::create() {
  return util::make_unique<MicroHttpdServerFactory>();
}

MicroHttpdServer::Response::Response(MHD_Connection* connection, int code,
                                     const IResponse::Headers& headers,
                                     int size,
                                     IResponse::ICallback::Pointer callback)
    : data_(std::make_shared<SharedData>()),
      connection_(connection),
      code_(code) {
  struct DataType {
    std::shared_ptr<SharedData> data_;
    MHD_Connection* connection_;
    IResponse::ICallback::Pointer callback_;
  };

  auto data_provider = [](void* cls, uint64_t, char* buf,
                          size_t max) -> ssize_t {
    auto data = static_cast<DataType*>(cls);
    std::unique_lock<std::mutex> lock(data->data_->mutex_);
    auto r = data->callback_->putData(buf, max);
    if (r == IResponse::ICallback::Suspend) {
      if (!data->data_->suspended_) {
        data->data_->suspended_ = true;
        MHD_suspend_connection(data->connection_);
      }
      return 0;
    } else if (r == IResponse::ICallback::Abort)
      return MHD_CONTENT_READER_END_WITH_ERROR;
    else if (r == IResponse::ICallback::End)
      return MHD_CONTENT_READER_END_OF_STREAM;
    else
      return r;
  };
  auto release_data = [](void* cls) {
    auto data = static_cast<DataType*>(cls);
    delete data;
  };
  auto data = util::make_unique<DataType>(
      DataType{data_, connection, std::move(callback)});
  response_ = MHD_create_response_from_callback(
      size == UnknownSize ? MHD_SIZE_UNKNOWN : size, CHUNK_SIZE, data_provider,
      data.release(), release_data);
  for (auto it : headers)
    MHD_add_response_header(response_, it.first.c_str(), it.second.c_str());
}

MicroHttpdServer::Response::~Response() {
  if (response_) MHD_destroy_response(response_);
}

void MicroHttpdServer::Response::resume() {
  std::unique_lock<std::mutex> lock(data_->mutex_);
  if (data_->suspended_) {
    data_->suspended_ = false;
    MHD_resume_connection(connection_);
  }
}

MicroHttpdServer::Request::Request(MHD_Connection* c, const char* url,
                                   const char* method)
    : connection_(c), url_(url), method_(method) {}

const char* MicroHttpdServer::Request::get(const std::string& name) const {
  return MHD_lookup_connection_value(connection_, MHD_GET_ARGUMENT_KIND,
                                     name.c_str());
}

const char* MicroHttpdServer::Request::header(const std::string& name) const {
  return MHD_lookup_connection_value(connection_, MHD_HEADER_KIND,
                                     name.c_str());
}

std::string MicroHttpdServer::Request::url() const { return url_; }

std::string MicroHttpdServer::Request::method() const { return method_; }

MicroHttpdServer::MicroHttpdServer(IHttpServer::ICallback::Pointer cb, int port)
    : http_server_(MHD_start_daemon(
          MHD_USE_POLL_INTERNALLY | MHD_USE_SUSPEND_RESUME, port, NULL, NULL,
          http_request_callback, this, MHD_OPTION_NOTIFY_COMPLETED,
          http_request_completed, this, MHD_OPTION_END)),
      callback_(cb) {}

MicroHttpdServer::~MicroHttpdServer() {
  if (http_server_) MHD_stop_daemon(http_server_);
}

MicroHttpdServer::IResponse::Pointer MicroHttpdServer::Request::response(
    int code, const IResponse::Headers& headers, int size,
    IResponse::ICallback::Pointer cb) const {
  return util::make_unique<Response>(connection_, code, headers, size,
                                     std::move(cb));
}

MicroHttpdServerFactory::MicroHttpdServerFactory() {
  MHD_set_panic_func(
      [](void*, const char* file, unsigned int line, const char* reason) {
        util::log(file, line, reason);
        exit(0);
      },
      nullptr);
}

IHttpServer::Pointer MicroHttpdServerFactory::create(
    IHttpServer::ICallback::Pointer cb, uint16_t port) {
  auto result = util::make_unique<MicroHttpdServer>(cb, port);
  if (result->valid())
    return result;
  else
    return nullptr;
}

IHttpServer::Pointer MicroHttpdServerFactory::create(
    IHttpServer::ICallback::Pointer cb, const std::string&,
    IHttpServer::Type type) {
  return create(cb, type == IHttpServer::Type::Authorization
                        ? AUTHORIZATION_PORT
                        : FILE_PROVIDER_PORT);
}

}  // namespace cloudstorage

#else

#include "IHttpServer.h"

namespace cloudstorage {
IHttpServerFactory::Pointer IHttpServerFactory::create() { return nullptr; }
}  // namespace cloudstorage

#endif  // WITH_MICROHTTPD
