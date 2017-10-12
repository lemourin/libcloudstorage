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
#include "Utility/Utility.h"
#include "gtest/gtest.h"

using namespace cloudstorage;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::Return;
using ::testing::InvokeArgument;
using ::testing::Invoke;
using ::testing::WithArgs;

class GoogleDriveTest : public ::testing::Test {
 public:
  void SetUp() {}

  void TearDown() {}
};

ACTION(CallSend) {
  Json::Value json;
  Json::Value item;
  item["kind"] = "drive#file";
  item["name"] = "test";
  json["files"].append(item);
  *arg2 << json;
  arg0(IHttpRequest::Response{IHttpRequest::Ok, {}, arg2, arg3});
}

TEST_F(GoogleDriveTest, ListDirectoryTest) {
  ICloudProvider::InitData data;
  data.http_engine_ = util::make_unique<HttpMock>();
  const HttpMock& http = static_cast<const HttpMock&>(*data.http_engine_);
  auto provider = ICloudStorage::create()->provider("google", std::move(data));
  auto request = std::make_shared<HttpRequestMock>();
  EXPECT_CALL(*request, setParameter(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request, setHeaderParameter(_, _)).Times(AtLeast(1));
  EXPECT_CALL(*request, send(_, _, _, _, _)).WillOnce(CallSend());
  EXPECT_CALL(http,
              create("https://www.googleapis.com/drive/v3/files", "GET", true))
      .WillRepeatedly(Return(request));
  auto r = provider->listDirectoryAsync(provider->rootDirectory())->result();
  ASSERT_NE(r.right(), nullptr);
  ASSERT_EQ(r.right()->size(), 1);
  ASSERT_EQ(r.right()->front()->filename(), "test");
}
