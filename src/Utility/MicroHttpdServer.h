/*****************************************************************************
 * MicroHttpdServer.h : interface of MicroHttpdServer
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

#ifndef MICRO_HTTPD_SERVER_H
#define MICRO_HTTPD_SERVER_H

#include <microhttpd.h>

#include "IHttpServer.h"

#include <mutex>

namespace cloudstorage {

class MicroHttpdServer : public IHttpServer {
 public:
  MicroHttpdServer(IHttpServer::ICallback::Pointer cb, int port);
  ~MicroHttpdServer();

  class Response : public IResponse {
   public:
    Response(MHD_Connection* connection, int code, const IResponse::Headers&,
             int size, IResponse::ICallback::Pointer);
    ~Response();

    MHD_Response* response() const { return response_; }
    int code() const { return code_; }

    CompletedCallback callback() const { return callback_; }
    void resume() override;
    void completed(CompletedCallback f) override { callback_ = f; }

   protected:
    struct SharedData {
      std::mutex mutex_;
      bool suspended_ = false;
    };
    std::shared_ptr<SharedData> data_;
    MHD_Connection* connection_;
    MHD_Response* response_;
    int code_;
    CompletedCallback callback_;
  };

  class Request : public IRequest {
   public:
    Request(MHD_Connection*, const char* url);

    MHD_Connection* connection() const { return connection_; }

    const char* get(const std::string& name) const override;
    const char* header(const std::string&) const override;
    std::string url() const override;

    IResponse::Pointer response(int code, const IResponse::Headers&, int size,
                                IResponse::ICallback::Pointer) const override;

   private:
    MHD_Connection* connection_;
    std::string url_;
  };

  ICallback::Pointer callback() const override { return callback_; }

 private:
  MHD_Daemon* http_server_;
  ICallback::Pointer callback_;
};

class MicroHttpdServerFactory : public IHttpServerFactory {
 public:
  IHttpServer::Pointer create(IHttpServer::ICallback::Pointer, uint16_t port);
  IHttpServer::Pointer create(IHttpServer::ICallback::Pointer,
                              const std::string& session_id,
                              IHttpServer::Type) override;
};

}  // namespace cloudstorage

#endif  // MICRO_HTTPD_SERVER_H
