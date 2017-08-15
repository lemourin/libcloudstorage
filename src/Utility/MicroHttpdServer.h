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

namespace cloudstorage {

class MicroHttpdServer : public IHttpServer {
 public:
  MicroHttpdServer(IHttpServer::ICallback::Pointer cb, int port);
  ~MicroHttpdServer();

  class Response : public IResponse {
   public:
    Response() = default;
    Response(int code, const IResponse::Headers&, const std::string& body);
    ~Response();

    MHD_Response* response() const { return response_; }
    int code() const { return code_; }

   protected:
    MHD_Response* response_;
    int code_;
  };

  class CallbackResponse : public Response {
   public:
    CallbackResponse(int code, const IResponse::Headers&, int size,
                     int chunk_size, IResponse::ICallback::Pointer);
  };

  class Connection : public IConnection {
   public:
    Connection(MHD_Connection*, const char* url);

    MHD_Connection* connection() const { return connection_; }
    CompletedCallback callback() const { return callback_; }

    const char* getParameter(const std::string& name) const override;
    const char* header(const std::string&) const override;
    std::string url() const override;
    void onCompleted(CompletedCallback) override;
    void suspend() override;
    void resume() override;

   private:
    MHD_Connection* connection_;
    std::string url_;
    CompletedCallback callback_;
  };

  IResponse::Pointer createResponse(int code, const IResponse::Headers&,
                                    const std::string& body) const override;
  IResponse::Pointer createResponse(
      int code, const IResponse::Headers&, int size, int chunk_size,
      IResponse::ICallback::Pointer) const override;

  ICallback::Pointer callback() const override { return callback_; }

 private:
  MHD_Daemon* http_server_;
  ICallback::Pointer callback_;
};

class MicroHttpdServerFactory : public IHttpServerFactory {
 public:
  IHttpServer::Pointer create(IHttpServer::ICallback::Pointer,
                              const std::string& session_id,
                              IHttpServer::Type) override;
};

}  // namespace cloudstorage

#endif  // MICRO_HTTPD_SERVER_H
