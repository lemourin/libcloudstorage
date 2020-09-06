/*****************************************************************************
 * HttpServerMock.h
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
#ifndef HTTPSERVERMOCK_H
#define HTTPSERVERMOCK_H

#include "IHttpServer.h"
#include "gmock/gmock.h"

class HttpServerMock : public cloudstorage::IHttpServer {
 public:
  class RequestMock : public cloudstorage::IHttpServer::IRequest {  // NOLINT
   public:
    MOCK_CONST_METHOD1(get, const char*(const std::string& name));     // NOLINT
    MOCK_CONST_METHOD1(header, const char*(const std::string& name));  // NOLINT
    MOCK_CONST_METHOD0(method, std::string());                         // NOLINT
    MOCK_CONST_METHOD0(url, std::string());                            // NOLINT
    MOCK_CONST_METHOD4(mocked_response,
                       IResponse::Pointer(int, const IResponse::Headers&, int,
                                          IResponse::ICallback*));
    IResponse::Pointer response(
        int code, const IResponse::Headers& headers, int64_t size,
        IResponse::ICallback::Pointer cb) const override {
      return mocked_response(code, headers, size, cb.get());
    }
  };

  MOCK_CONST_METHOD0(callback, ICallback::Pointer());
};

class HttpServerFactoryMock : public cloudstorage::IHttpServerFactory {
 public:
  MOCK_METHOD3(create, cloudstorage::IHttpServer::Pointer(
                           cloudstorage::IHttpServer::ICallback::Pointer,
                           const std::string& session_id,
                           cloudstorage::IHttpServer::Type));
};

#endif  // HTTPSERVERMOCK_H
