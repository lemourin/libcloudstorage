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
#include "ICloudStorage.h"
#include "Utility/CloudFactoryMock.h"
#include "Utility/CloudProviderMock.h"
#include "Utility/HttpMock.h"
#include "Utility/HttpServerMock.h"
#include "Utility/Item.h"
#include "Utility/ThreadPoolMock.h"
#include "Utility/Utility.h"
#include "gtest/gtest.h"

namespace cloudstorage {

using ::testing::_;
using ::testing::AllOf;
using ::testing::AtLeast;
using ::testing::Field;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;
using ::testing::StrEq;
using ::testing::Truly;

class AuthCallback : public ICloudProvider::IAuthCallback {
  Status userConsentRequired(const ICloudProvider&) override {
    return Status::WaitForAuthorizationCode;
  }

  void done(const ICloudProvider&, EitherError<void>) override {}
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

ACTION(CreateServer) {  // NOLINT
  auto server = util::make_unique<HttpServerMock>();
  auto request = util::make_unique<HttpServerMock::RequestMock>();
  EXPECT_CALL(*request, get(_)).WillRepeatedly(Return(nullptr));
  EXPECT_CALL(*request, get("state")).WillRepeatedly(Return("DEFAULT_STATE"));
  EXPECT_CALL(*request, get("code")).WillRepeatedly(Return("CODE"));
  EXPECT_CALL(*request, get("error")).WillRepeatedly(Return(nullptr));
  EXPECT_CALL(*request, get("accepted")).WillRepeatedly(Return("true"));
  EXPECT_CALL(*request, mocked_response(IHttpRequest::Ok, _, _, _));
  EXPECT_CALL(*server, callback()).WillRepeatedly(Return(arg0));
  arg0->handle(*request);
  return server;  // NOLINT
}

ACTION(CreateFileServer) { return util::make_unique<HttpServerMock>(); }

TEST(GoogleDriveTest, ListDirectoryTest) {
  ICloudProvider::InitData data;
  data.http_engine_ = util::make_unique<HttpMock>();
  data.callback_ = util::make_unique<AuthCallback>();
  data.thread_pool_ = util::make_unique<ThreadPoolMock>();
  data.thumbnailer_thread_pool = util::make_unique<ThreadPoolMock>();
  const auto& http = static_cast<const HttpMock&>(*data.http_engine_);
  auto provider = ICloudStorage::create()->provider("google", std::move(data));
  auto request = request_mock();
  EXPECT_CALL(*request, send).WillOnce(CallSend());
  EXPECT_CALL(http,
              create("https://www.googleapis.com/drive/v3/files", "GET", true))
      .WillRepeatedly(Return(request));
  auto r =
      provider->listDirectorySimpleAsync(provider->rootDirectory())->result();
  ASSERT_NE(r.right(), nullptr);
  ASSERT_EQ(r.right()->size(), 2);
  ASSERT_EQ(r.right()->front()->filename(), "test");
}

TEST(GoogleDriveTest, AuthorizationTest) {
  ICloudProvider::InitData data;
  data.http_engine_ = util::make_unique<HttpMock>();
  data.http_server_ = util::make_unique<HttpServerFactoryMock>();
  data.thread_pool_ = util::make_unique<ThreadPoolMock>();
  data.thumbnailer_thread_pool = util::make_unique<ThreadPoolMock>();
  data.callback_ = util::make_unique<AuthCallback>();
  const auto& http = static_cast<const HttpMock&>(*data.http_engine_);
  auto& http_factory = static_cast<HttpServerFactoryMock&>(*data.http_server_);
  EXPECT_CALL(http_factory, create(_, _, IHttpServer::Type::FileProvider))
      .WillOnce(CreateFileServer());
  auto provider = ICloudStorage::create()->provider("google", std::move(data));
  auto request = request_mock();
  EXPECT_CALL(*request, send).WillOnce(UnauthorizedSend());
  auto authorized_request = std::make_shared<HttpRequestMock>();
  EXPECT_CALL(*authorized_request, setParameter).Times(AtLeast(1));
  EXPECT_CALL(*authorized_request,
              setHeaderParameter("Authorization", "Bearer access_token"));
  EXPECT_CALL(*authorized_request, send).WillOnce(CallSend());
  EXPECT_CALL(http,
              create("https://www.googleapis.com/drive/v3/files", "GET", true))
      .WillOnce(Return(request))
      .WillOnce(Return(authorized_request));
  auto token_request = request_mock();
  EXPECT_CALL(*token_request, send).WillOnce(UnauthorizedSend());
  auto next_token_request = request_mock();
  EXPECT_CALL(*next_token_request, send).WillOnce(AuthorizedSend());
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

TEST(GoogleDriveTest, GetsItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("google", {});

  ExpectHttp(mock.http(), "https://www.googleapis.com/drive/v3/files/item")
      .WithParameter("fields",
                     "id,name,thumbnailLink,trashed,mimeType,"
                     "iconLink,parents,size,modifiedTime")
      .WillRespondWith(R"js({ "name": "filename" })js");

  ExpectImmediatePromise(provider->getItemData("item"),
                         Pointee(Property(&IItem::filename, "filename")));
}

TEST(GoogleDriveTest, PatchesIconLink) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("google", {});

  ExpectHttp(mock.http(), "https://www.googleapis.com/drive/v3/files/item")
      .WithParameter("fields",
                     "id,name,thumbnailLink,trashed,mimeType,"
                     "iconLink,parents,size,modifiedTime")
      .WillRespondWith(R"js({ "iconLink": "icon?size=16" })js");

  ExpectImmediatePromise(
      provider->getItemData("item"), Pointee(Truly([](const IItem& item) {
        return static_cast<const Item&>(item).thumbnail_url() ==
               "icon?size=256";
      })));
}

TEST(GoogleDriveTest, GetsGeneralData) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("google", {});

  ExpectHttp(mock.http(), "https://www.googleapis.com/drive/v3/about")
      .WillRespondWith(R"js({
                              "name": "filename",
                              "user": { "emailAddress": "admin@admin.ru" },
                              "storageQuota": { "usage": "10", "limit": "100" }
                            })js");

  ExpectImmediatePromise(provider->generalData(),
                         AllOf(Field(&GeneralData::username_, "admin@admin.ru"),
                               Field(&GeneralData::space_used_, 10),
                               Field(&GeneralData::space_total_, 100)));
}

TEST(GoogleDriveTest, GetsItemUrl) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("google", {});

  const auto expected_url =
      "https://www.googleapis.com/drive/v3/files/item?alt=media&access_token=";

  ExpectHttp(mock.http(), "https://www.googleapis.com/drive/v3/files/item")
      .WillRespondWithCode(200);

  ExpectHttp(mock.http(), expected_url)
      .WithMethod("HEAD")
      .WillRespondWithCode(200);

  ExpectImmediatePromise(
      provider->getFileUrl(util::make_unique<Item>(
          "item", "item", IItem::UnknownSize, IItem::UnknownTimeStamp,
          IItem::FileType::Unknown)),
      StrEq(expected_url));
}

TEST(GoogleDriveTest, GetsItemUrlForGoogleTypeFile) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("google", {});

  const auto expected_url =
      "https://www.googleapis.com/drive/v3/files/item/"
      "export?access_token=&mimeType=application/"
      "vnd.openxmlformats-officedocument.wordprocessingml.document";

  ExpectHttp(mock.http(), "https://www.googleapis.com/drive/v3/files/item")
      .WillRespondWithCode(200);

  ExpectHttp(mock.http(), expected_url)
      .WithMethod("HEAD")
      .WillRespondWithCode(200);

  auto item =
      std::make_shared<Item>("item", "item", IItem::UnknownSize,
                             IItem::UnknownTimeStamp, IItem::FileType::Unknown);
  item->set_mime_type("application/vnd.google-apps.document");

  ExpectImmediatePromise(provider->getFileUrl(item), StrEq(expected_url));
}

TEST(GoogleDriveTest, CreatesDirectory) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("google", {});

  ExpectHttp(mock.http(), "https://www.googleapis.com/drive/v3/files")
      .WithMethod("POST")
      .WithHeaderParameter("Content-Type", "application/json")
      .WithHeaderParameter("Authorization", _)
      .WithParameter("fields",
                     "id,name,thumbnailLink,trashed,"
                     "mimeType,iconLink,parents,size,modifiedTime")
      .WithBody(IgnoringWhitespace(
          R"js({
                 "mimeType": "application/vnd.google-apps.folder",
                 "name": "directory",
                 "parents": [ "id" ]
               })js"))
      .WillRespondWith(R"js({ "name": "directory" })js");

  auto item = std::make_shared<Item>("item", "id", IItem::UnknownSize,
                                     IItem::UnknownTimeStamp,
                                     IItem::FileType::Directory);

  ExpectImmediatePromise(
      provider->createDirectory(item, "directory"),
      Pointee(AllOf(Property(&IItem::filename, "directory"))));
}

TEST(GoogleDriveTest, MovesItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("google", {});

  ExpectHttp(mock.http(), "https://www.googleapis.com/drive/v3/files/srcid")
      .WithMethod("PATCH")
      .WithHeaderParameter("Content-Type", "application/json")
      .WithHeaderParameter("Authorization", _)
      .WithParameter("fields",
                     "id,name,thumbnailLink,trashed,"
                     "mimeType,iconLink,parents,size,modifiedTime")
      .WithParameter("addParents", "dstid")
      .WithParameter("removeParents", "srcparent")
      .WillRespondWith(R"js({ "name": "src" })js");

  auto source = std::make_shared<Item>("src", "srcid", IItem::UnknownSize,
                                       IItem::UnknownTimeStamp,
                                       IItem::FileType::Directory);
  source->set_parents({"srcparent"});
  auto destination = std::make_shared<Item>("dst", "dstid", IItem::UnknownSize,
                                            IItem::UnknownTimeStamp,
                                            IItem::FileType::Directory);

  ExpectImmediatePromise(provider->moveItem(source, destination),
                         Pointee(AllOf(Property(&IItem::filename, "src"))));
}

TEST(GoogleDriveTest, RenamesItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("google", {});

  ExpectHttp(mock.http(), "https://www.googleapis.com/drive/v3/files/id")
      .WithMethod("PATCH")
      .WithHeaderParameter("Content-Type", "application/json")
      .WithHeaderParameter("Authorization", _)
      .WithParameter("fields",
                     "id,name,thumbnailLink,trashed,"
                     "mimeType,iconLink,parents,size,modifiedTime")
      .WithBody(IgnoringWhitespace(R"js({ "name": "new_name" })js"))
      .WillRespondWith(R"js({ "name": "new_name" })js");

  auto item = std::make_shared<Item>("item", "id", IItem::UnknownSize,
                                     IItem::UnknownTimeStamp,
                                     IItem::FileType::Directory);

  ExpectImmediatePromise(
      provider->renameItem(item, "new_name"),
      Pointee(AllOf(Property(&IItem::filename, "new_name"))));
}

TEST(GoogleDriveTest, DeletesItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("google", {});

  ExpectHttp(mock.http(), "https://www.googleapis.com/drive/v3/files/id")
      .WithMethod("DELETE")
      .WillRespondWithCode(200);

  ExpectImmediatePromise(provider->deleteItem(std::make_shared<Item>(
      "item", "id", IItem::UnknownSize, IItem::UnknownTimeStamp,
      IItem::FileType::Directory)));
}

TEST(GoogleDriveTest, DownloadsItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("google", {});

  const std::string expected_content = "content";

  ExpectHttp(mock.http(), "https://www.googleapis.com/drive/v3/files/id")
      .WithParameter("alt", "media")
      .WillRespondWith(expected_content.c_str());

  auto download_callback = std::make_shared<DownloadCallbackMock>();
  EXPECT_CALL(*download_callback, receivedData)
      .With(AllArgs(Truly([&](const std::tuple<const char*, uint32_t>& tuple) {
        const char* data = std::get<0>(tuple);
        uint32_t size = std::get<1>(tuple);
        return size == expected_content.size() &&
               std::string(data, size) == expected_content;
      })));

  auto item =
      std::make_shared<Item>("item", "id", IItem::UnknownSize,
                             IItem::UnknownTimeStamp, IItem::FileType::Unknown);
  ExpectImmediatePromise(
      provider->downloadFile(item, FullRange, download_callback));
}

TEST(GoogleDriveTest, FailsPartialDownloadForExportedItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("google", {});

  auto item =
      std::make_shared<Item>("item", "id", IItem::UnknownSize,
                             IItem::UnknownTimeStamp, IItem::FileType::Unknown);
  item->set_mime_type("application/vnd.google-apps.document");

  ExpectFailedPromise(
      provider->downloadFile(item, {0, 1},
                             std::make_shared<DownloadCallbackMock>()),
      Field(&Error::code_, IHttpRequest::ServiceUnavailable));
}

TEST(GoogleDriveTest, DownloadsExportedItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("google", {});

  const std::string expected_content = "content";

  ExpectHttp(mock.http(), "https://www.googleapis.com/drive/v3/files/id/export")
      .WithParameter(
          "mimeType",
          "application/"
          "vnd.openxmlformats-officedocument.wordprocessingml.document")
      .WillRespondWith(expected_content.c_str());

  auto download_callback = std::make_shared<DownloadCallbackMock>();
  EXPECT_CALL(*download_callback, receivedData)
      .With(AllArgs(Truly([&](const std::tuple<const char*, uint32_t>& tuple) {
        const char* data = std::get<0>(tuple);
        uint32_t size = std::get<1>(tuple);
        return size == expected_content.size() &&
               std::string(data, size) == expected_content;
      })));

  auto item =
      std::make_shared<Item>("item", "id", IItem::UnknownSize,
                             IItem::UnknownTimeStamp, IItem::FileType::Unknown);
  item->set_mime_type("application/vnd.google-apps.document");

  ExpectImmediatePromise(
      provider->downloadFile(item, FullRange, download_callback));
}

TEST(GoogleDriveTest, UploadsItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("google", {});

  ExpectHttp(mock.http(), "https://www.googleapis.com/upload/drive/v3/files")
      .WithMethod("POST")
      .WithHeaderParameter("Authorization", _)
      .WithHeaderParameter("Content-Type",
                           "multipart/related; boundary=fWoDm9QNn3v3Bq3bScUX")
      .WithParameter("fields",
                     "id,name,thumbnailLink,trashed,"
                     "mimeType,iconLink,parents,size,modifiedTime")
      .WithParameter("uploadType", "multipart")
      .WithBody(
          "--fWoDm9QNn3v3Bq3bScUX\r\nContent-Type: application/json; "
          "charset=UTF-8\r\n\r\n{\"name\":\"filename.txt\",\"parents\":["
          "\"id\"]}\r\n--fWoDm9QNn3v3Bq3bScUX\r\nContent-Type: "
          "\r\n\r\ncontent\r\n--fWoDm9QNn3v3Bq3bScUX--\r\n")
      .WillRespondWith(R"js({ "name": "filename.txt" })js");

  ExpectHttp(mock.http(), "https://www.googleapis.com/drive/v3/files")
      .WillRespondWith("{}");

  auto item = std::make_shared<Item>("item", "id", IItem::UnknownSize,
                                     IItem::UnknownTimeStamp,
                                     IItem::FileType::Directory);

  auto stream = std::make_shared<std::stringstream>();
  *stream << "content";
  ExpectImmediatePromise(
      provider->uploadFile(item, "filename.txt",
                           provider->streamUploader(stream)),
      Pointee(AllOf(Property(&IItem::filename, "filename.txt"))));
}

TEST(GoogleDriveTest, PatchesAlreadyPresentFile) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("google", {});

  ExpectHttp(mock.http(),
             "https://www.googleapis.com/upload/drive/v3/files/fileid")
      .WithMethod("PATCH")
      .WithHeaderParameter("Content-Type",
                           "multipart/related; boundary=fWoDm9QNn3v3Bq3bScUX")
      .WithHeaderParameter("Authorization", _)
      .WithParameter("uploadType", "multipart")
      .WithParameter("fields",
                     "id,name,thumbnailLink,trashed,"
                     "mimeType,iconLink,parents,size,modifiedTime")
      .WithBody(
          "--fWoDm9QNn3v3Bq3bScUX\r\nContent-Type: application/json; "
          "charset=UTF-8\r\n\r\nnull\r\n--fWoDm9QNn3v3Bq3bScUX\r\nContent-Type:"
          " \r\n\r\ncontent\r\n--fWoDm9QNn3v3Bq3bScUX--\r\n")
      .WillRespondWith(R"js({ "name": "filename.txt" })js");

  ExpectHttp(mock.http(), "https://www.googleapis.com/drive/v3/files")
      .WillRespondWith(R"({
                            "files": [{
                              "name": "filename.txt",
                              "id": "fileid"
                            }]
                          })");

  auto item = std::make_shared<Item>("item", "id", IItem::UnknownSize,
                                     IItem::UnknownTimeStamp,
                                     IItem::FileType::Directory);

  auto stream = std::make_shared<std::stringstream>();
  *stream << "content";
  ExpectImmediatePromise(
      provider->uploadFile(item, "filename.txt",
                           provider->streamUploader(stream)),
      Pointee(AllOf(Property(&IItem::filename, "filename.txt"))));
}

TEST(GoogleDriveTest, ReturnsAuthorizeLibraryUrl) {
  auto mock = CloudFactoryMock::create();
  ICloudFactory::ProviderInitData init_data;
  init_data.hints_["state"] = "state";
  EXPECT_THAT(mock.factory()->authorizationUrl("google", init_data),
              StrEq("https://accounts.google.com/o/oauth2/"
                    "auth?response_type=code&client_id=646432077068-"
                    "hmvk44qgo6d0a64a5h9ieue34p3j2dcv.apps.googleusercontent."
                    "com&redirect_uri=http://cloudstorage-test/"
                    "google&scope=https://www.googleapis.com/auth/"
                    "drive&access_type=offline&prompt=consent&state=state"));
}

TEST(GoogleDriveTest, RefreshesToken) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("google", {});

  ExpectHttp(mock.http(), "https://www.googleapis.com/drive/v3/about")
      .WillRespondWithCode(401)
      .AndThen()
      .WithHeaderParameter("Authorization", "Bearer token")
      .WillRespondWith(R"js({
                              "name": "filename",
                              "user": { "emailAddress": "admin@admin.ru" },
                              "storageQuota": { "usage": "10", "limit": "100" }
                            })js");

  ExpectHttp(mock.http(), "https://accounts.google.com/o/oauth2/token")
      .WithMethod("POST")
      .WillRespondWith(R"js({ "access_token": "token" })js");

  ExpectImmediatePromise(provider->generalData(), _);
}

}  // namespace cloudstorage
