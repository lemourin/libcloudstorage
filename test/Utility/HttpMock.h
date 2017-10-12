/*****************************************************************************
 * HttpMock.h
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

#include "IHttp.h"
#include "gmock/gmock.h"

using namespace cloudstorage;

class HttpRequestMock : public IHttpRequest {
 public:
  MOCK_METHOD2(setParameter,
               void(const std::string& parameter, const std::string& value));

  MOCK_METHOD2(setHeaderParameter,
               void(const std::string& parameter, const std::string& value));

  MOCK_CONST_METHOD0(parameters, const GetParameters&());

  MOCK_CONST_METHOD0(headerParameters, const HeaderParameters&());

  MOCK_CONST_METHOD0(url, const std::string&());

  MOCK_CONST_METHOD0(method, const std::string&());

  MOCK_CONST_METHOD0(follow_redirect, bool());

  MOCK_CONST_METHOD5(send, void(CompleteCallback on_completed,
                                std::shared_ptr<std::istream> data,
                                std::shared_ptr<std::ostream> response,
                                std::shared_ptr<std::ostream> error_stream,
                                ICallback::Pointer callback));
};

class HttpMock : public IHttp {
 public:
  MOCK_CONST_METHOD3(create, IHttpRequest::Pointer(const std::string& url,
                                                   const std::string& method,
                                                   bool follow_redirect));
};
