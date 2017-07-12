/*****************************************************************************
 * IHttpServer.h : interface of IHttpServer
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
#ifndef IHTTP_SERVER_H
#define IHTTP_SERVER_H

#include <memory>
#include <string>
#include <unordered_map>

namespace cloudstorage {

class IHttpServer {
 public:
  using Pointer = std::unique_ptr<IHttpServer>;

  enum class Type { MultiThreaded, SingleThreaded };

  class IConnection;

  virtual ~IHttpServer() = default;

  class IResponse {
   public:
    using Pointer = std::unique_ptr<IResponse>;
    using Headers = std::unordered_map<std::string, std::string>;

    class ICallback {
     public:
      using Pointer = std::unique_ptr<ICallback>;

      virtual int putData(char* buffer, size_t size) = 0;
    };

    virtual ~IResponse() = default;

    virtual void send(const IConnection&) = 0;
  };

  class IConnection {
   public:
    virtual ~IConnection() = default;

    virtual const char* getParameter(const std::string& name) const = 0;
    virtual std::string url() const = 0;
  };

  class ICallback {
   public:
    using Pointer = std::shared_ptr<ICallback>;

    virtual ~ICallback() = default;

    virtual IResponse::Pointer receivedConnection(const IHttpServer&,
                                                  const IConnection&) = 0;
  };

  virtual IResponse::Pointer createResponse(int code, const IResponse::Headers&,
                                            const std::string& body) const = 0;

  virtual IResponse::Pointer createResponse(
      int code, const IResponse::Headers&, int size, int chunk_size,
      IResponse::ICallback::Pointer) const = 0;
};

class IHttpServerFactory {
 public:
  using Pointer = std::shared_ptr<IHttpServerFactory>;

  virtual ~IHttpServerFactory() = default;

  virtual IHttpServer::Pointer create(IHttpServer::ICallback::Pointer,
                                      IHttpServer::Type, int port) = 0;
};

}  // namespace cloudstorage

#endif  // IHTTP_SERVER_H
