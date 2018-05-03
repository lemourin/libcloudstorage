/*****************************************************************************
 * GoogleDriveTest.cpp
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
#include <json/json.h>
#include "ICloudStorage.h"
#include "Utility/HttpMock.h"
#include "Utility/HttpServerMock.h"
#include "Utility/Utility.h"
#include "gtest/gtest.h"

using namespace cloudstorage;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::ByMove;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::InvokeArgument;
using ::testing::Return;
using ::testing::WithArgs;

class AuthCallback : public ICloudProvider::IAuthCallback {
  Status userConsentRequired(const ICloudProvider&) override {
    return Status::WaitForAuthorizationCode;
  }

  void done(const ICloudProvider&, EitherError<void>) override {}
};

class GoogleDriveTest : public ::testing::Test {
 public:
  void SetUp() {}

  void TearDown() {}
};

std::shared_ptr<HttpRequestMock> request_mock() {
  auto request = std::make_shared<HttpRequestMock>();
  EXPECT_CALL(*request, setParameter(_, _)).Times(AtLeast(0));
  EXPECT_CALL(*request, setHeaderParameter(_, _)).Times(AtLeast(0));
  return request;
}

ACTION(CallSend) {
  Json::Value json;
  Json::Value item;
  item["kind"] = "drive#file";
  item["name"] = "test";
  json["files"].append(item);
  *arg2 << json;
  arg0(IHttpRequest::Response{IHttpRequest::Ok, {}, arg2, arg3});
}

ACTION(UnauthorizedSend) {
  Json::Value json;
  arg0(IHttpRequest::Response{IHttpRequest::Unauthorized, {}, arg2, arg3});
}

ACTION(AuthorizedSend) {
  Json::Value json;
  json["access_token"] = "access_token";
  json["refresh_token"] = "refresh_token";
  json["expires"] = "-1";
  *arg2 << json;
  std::stringstream stream;
  stream << arg1->rdbuf();
  ASSERT_NE(stream.str().find("code=CODE"), std::string::npos);
  arg0(IHttpRequest::Response{IHttpRequest::Ok, {}, arg2, arg3});
}

ACTION(CreateServer) {
  auto server = util::make_unique<HttpServerMock>();
  auto request = util::make_unique<HttpServerMock::RequestMock>();
  EXPECT_CALL(*request, get("state")).WillRepeatedly(Return("DEFAULT_STATE"));
  EXPECT_CALL(*request, get("code")).WillRepeatedly(Return("CODE"));
  EXPECT_CALL(*request, get("error")).WillRepeatedly(Return(nullptr));
  EXPECT_CALL(*request, get("accepted")).WillRepeatedly(Return("true"));
  EXPECT_CALL(*request, mocked_response(IHttpRequest::Ok, _, _, _));
  EXPECT_CALL(*server, callback()).WillRepeatedly(Return(arg0));
  arg0->handle(*request);
  return std::move(server);
}

ACTION(CreateFileServer) { return util::make_unique<HttpServerMock>(); }

TEST_F(GoogleDriveTest, ListDirectoryTest) {
  ICloudProvider::InitData data;
  data.http_engine_ = util::make_unique<HttpMock>();
  data.callback_ = util::make_unique<AuthCallback>();
  const HttpMock& http = static_cast<const HttpMock&>(*data.http_engine_);
  auto provider = ICloudStorage::create()->provider("google", std::move(data));
  auto request = request_mock();
  EXPECT_CALL(*request, send(_, _, _, _, _)).WillOnce(CallSend());
  EXPECT_CALL(http,
              create("https://www.googleapis.com/drive/v3/files", "GET", true))
      .WillRepeatedly(Return(request));
  auto r =
      provider->listDirectorySimpleAsync(provider->rootDirectory())->result();
  ASSERT_NE(r.right(), nullptr);
  ASSERT_EQ(r.right()->size(), 2);
  ASSERT_EQ(r.right()->front()->filename(), "test");
}

TEST_F(GoogleDriveTest, AuthorizationTest) {
  ICloudProvider::InitData data;
  data.http_engine_ = util::make_unique<HttpMock>();
  data.http_server_ = util::make_unique<HttpServerFactoryMock>();
  data.callback_ = util::make_unique<AuthCallback>();
  const HttpMock& http = static_cast<const HttpMock&>(*data.http_engine_);
  HttpServerFactoryMock& http_factory =
      static_cast<HttpServerFactoryMock&>(*data.http_server_);
  EXPECT_CALL(http_factory, create(_, _, IHttpServer::Type::FileProvider))
      .WillOnce(CreateFileServer());
  auto provider = ICloudStorage::create()->provider("google", std::move(data));
  auto request = request_mock();
  EXPECT_CALL(*request, send(_, _, _, _, _)).WillOnce(UnauthorizedSend());
  auto authorized_request = std::make_shared<HttpRequestMock>();
  EXPECT_CALL(*authorized_request, setParameter(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*authorized_request,
              setHeaderParameter("Authorization", "Bearer access_token"));
  EXPECT_CALL(*authorized_request, send(_, _, _, _, _)).WillOnce(CallSend());
  EXPECT_CALL(http,
              create("https://www.googleapis.com/drive/v3/files", "GET", true))
      .WillOnce(Return(request))
      .WillOnce(Return(authorized_request));
  auto token_request = request_mock();
  EXPECT_CALL(*token_request, send(_, _, _, _, _)).WillOnce(UnauthorizedSend());
  auto next_token_request = request_mock();
  EXPECT_CALL(*next_token_request, send(_, _, _, _, _))
      .WillOnce(AuthorizedSend());
  EXPECT_CALL(
      http, create("https://accounts.google.com/o/oauth2/token", "POST", true))
      .WillOnce(Return(token_request))
      .WillOnce(Return(next_token_request));
  EXPECT_CALL(http_factory, create(_, _, IHttpServer::Type::Authorization))
      .WillRepeatedly(CreateServer());
  auto r =
      provider->listDirectorySimpleAsync(provider->rootDirectory())->result();
  if (r.left()) util::log(r.left()->code_, r.left()->description_);
  ASSERT_NE(r.right(), nullptr);
  ASSERT_EQ(r.right()->size(), 2);
  ASSERT_EQ(r.right()->front()->filename(), "test");
}
