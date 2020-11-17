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
#include "Utility/Item.h"
#include "Utility/ThreadPoolMock.h"
#include "Utility/Utility.h"
#include "gtest/gtest.h"

namespace cloudstorage {

using ::testing::_;
using ::testing::AllOf;
using ::testing::AtLeast;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;
using ::testing::StrEq;
using ::testing::Truly;

TEST(GooglePhotosTest, GetsGeneralData) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("gphotos", {});

  ExpectHttp(mock.http(),
             "https://accounts.google.com/.well-known/openid-configuration")
      .WillRespondWith(R"js({ "userinfo_endpoint": "userinfo-endpoint" })js");

  ExpectHttp(mock.http(), "userinfo-endpoint")
      .WillRespondWith(R"js({ "email": "admin@admin.ru" })js");

  ExpectImmediatePromise(provider->generalData(),
                         AllOf(Field(&GeneralData::username_, "admin@admin.ru"),
                               Field(&GeneralData::space_used_, 0),
                               Field(&GeneralData::space_total_, 0)));
}

TEST(GooglePhotosTest, GetsItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("gphotos", {});

  ExpectHttp(mock.http(),
             "https://photoslibrary.googleapis.com/v1/mediaItems/item")
      .WillRespondWith(R"js({
                              "filename": "filename",
                              "id": "id",
                              "mediaMetadata": {
                                "creationTime": "2012-04-12T04:06:12.4213Z",
                                "photo": "jpg"
                              }
                            })js");

  ExpectImmediatePromise(
      provider->getItemData("item"),
      Pointee(AllOf(
          Property(&IItem::filename, "filename"), Property(&IItem::id, "id"),
          Property(&IItem::size, IItem::UnknownSize),
          Property(&IItem::timestamp,
                   std::chrono::system_clock::from_time_t(1334203572)),
          Property(&IItem::type, IItem::FileType::Image))));
}

TEST(GooglePhotosTest, GetsProcessingItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("gphotos", {});

  ExpectHttp(mock.http(),
             "https://photoslibrary.googleapis.com/v1/mediaItems/item")
      .WillRespondWith(
          R"js({
                 "filename": "filename",
                 "mediaMetadata": { "video": { "status": "PROCESSING" } }
               })js");

  ExpectImmediatePromise(
      provider->getItemData("item"),
      Pointee(AllOf(Property(&IItem::filename, "[PROCESSING] filename"),
                    Property(&IItem::type, IItem::FileType::Video))));
}

TEST(GooglePhotosTest, GetsAlbum) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("gphotos", {});

  ExpectHttp(mock.http(),
             "https://photoslibrary.googleapis.com/v1/mediaItems/item")
      .WillRespondWith(R"js({ "title": "filename", "id": "id" })js");

  ExpectImmediatePromise(
      provider->getItemData("item"),
      Pointee(AllOf(Property(&IItem::filename, "filename"),
                    Property(&IItem::id, "id"),
                    Property(&IItem::size, IItem::UnknownSize),
                    Property(&IItem::timestamp, IItem::UnknownTimeStamp),
                    Property(&IItem::type, IItem::FileType::Directory))));
}

TEST(GooglePhotosTest, GetsItemUrl) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("gphotos", {});

  ExpectHttp(mock.http(),
             "https://photoslibrary.googleapis.com/v1/mediaItems/item")
      .WillRespondWith(R"js({
                              "baseUrl": "url",
                              "mediaMetadata": { "video": "mp4" }
                            })js");

  ExpectHttp(mock.http(), "url=dv").WithMethod("HEAD").WillRespondWithCode(200);

  ExpectImmediatePromise(
      provider->getFileUrl(util::make_unique<Item>(
          "item", "item", IItem::UnknownSize, IItem::UnknownTimeStamp,
          IItem::FileType::Unknown)),
      StrEq("url=dv"));
}

TEST(GooglePhotosTest, ListsRootDirectory) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("gphotos", {});

  ExpectImmediatePromise(
      provider->listDirectory(provider->root()),
      ElementsAre(
          Pointee(AllOf(Property(&IItem::id, "photos"),
                        Property(&IItem::filename, "Photos"))),
          Pointee(AllOf(Property(&IItem::id, "albums"),
                        Property(&IItem::filename, "Albums"))),
          Pointee(AllOf(Property(&IItem::id, "shared"),
                        Property(&IItem::filename, "Shared with me")))));
}

TEST(GooglePhotosTest, ListsAlbums) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("gphotos", {});

  ExpectHttp(mock.http(), "https://photoslibrary.googleapis.com/v1/albums")
      .WillRespondWith(
          R"({ "albums": [{ "filename": "filename", "id": "id" }] })");

  auto item = std::make_shared<Item>("item", "albums", IItem::UnknownSize,
                                     IItem::UnknownTimeStamp,
                                     IItem::FileType::Directory);

  ExpectImmediatePromise(
      provider->listDirectory(item),
      ElementsAre(Pointee(AllOf(Property(&IItem::id, "id"),
                                Property(&IItem::filename, "filename")))));
}

TEST(GooglePhotosTest, ListsSharedAlbums) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("gphotos", {});

  ExpectHttp(mock.http(),
             "https://photoslibrary.googleapis.com/v1/sharedAlbums")
      .WillRespondWith(
          R"({ "sharedAlbums": [{ "filename": "filename", "id": "id" }] })");

  auto item = std::make_shared<Item>("item", "shared", IItem::UnknownSize,
                                     IItem::UnknownTimeStamp,
                                     IItem::FileType::Directory);

  ExpectImmediatePromise(
      provider->listDirectory(item),
      ElementsAre(Pointee(AllOf(Property(&IItem::id, "id"),
                                Property(&IItem::filename, "filename")))));
}

TEST(GooglePhotosTest, ListsPhotos) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("gphotos", {});

  ExpectHttp(mock.http(), "https://photoslibrary.googleapis.com/v1/mediaItems")
      .WillRespondWith(
          R"({ "mediaItems": [{ "filename": "filename", "id": "id" }] })");

  auto item = std::make_shared<Item>("item", "photos", IItem::UnknownSize,
                                     IItem::UnknownTimeStamp,
                                     IItem::FileType::Directory);

  ExpectImmediatePromise(
      provider->listDirectory(item),
      ElementsAre(Pointee(AllOf(Property(&IItem::id, "id"),
                                Property(&IItem::filename, "filename")))));
}

TEST(GooglePhotosTest, ListsAlbum) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("gphotos", {});

  ExpectHttp(mock.http(),
             "https://photoslibrary.googleapis.com/v1/mediaItems:search")
      .WithMethod("POST")
      .WithHeaderParameter("Content-Type", "application/json")
      .WithHeaderParameter("Authorization", _)
      .WithBody(IgnoringWhitespace(R"js({ "albumId": "album_id" })js"))
      .WillRespondWith(
          R"({ "mediaItems": [{ "filename": "filename", "id": "id" }] })");

  auto item = std::make_shared<Item>("item", "album_id", IItem::UnknownSize,
                                     IItem::UnknownTimeStamp,
                                     IItem::FileType::Directory);

  ExpectImmediatePromise(
      provider->listDirectory(item),
      ElementsAre(Pointee(AllOf(Property(&IItem::id, "id"),
                                Property(&IItem::filename, "filename")))));
}

TEST(GooglePhotosTest, CreatesDirectory) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("gphotos", {});

  ExpectHttp(mock.http(), "https://photoslibrary.googleapis.com/v1/albums")
      .WithMethod("POST")
      .WithHeaderParameter("Content-Type", "application/json")
      .WithHeaderParameter("Authorization", _)
      .WithBody(
          IgnoringWhitespace(R"js({ "album": { "title": "directory" } })js"))
      .WillRespondWith(R"js({ "title": "directory" })js");

  auto item = std::make_shared<Item>("item", "albums", IItem::UnknownSize,
                                     IItem::UnknownTimeStamp,
                                     IItem::FileType::Directory);

  ExpectImmediatePromise(
      provider->createDirectory(item, "directory"),
      Pointee(AllOf(Property(&IItem::filename, "directory"))));
}

TEST(GooglePhotosTest, GetsThumbnail) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("gphotos", {});

  const std::string expected_content = "content";

  ExpectHttp(mock.http(),
             "https://photoslibrary.googleapis.com/v1/mediaItems/id")
      .WillRespondWith(R"js({ "baseUrl": "url" })js");

  ExpectHttp(mock.http(), "url=w128-h128-c")
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
  ExpectImmediatePromise(provider->downloadThumbnail(item, download_callback));
}

TEST(GooglePhotosTest, UploadsItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("gphotos", {});

  ExpectHttp(mock.http(), "https://photoslibrary.googleapis.com/v1/uploads")
      .WithMethod("POST")
      .WithHeaderParameter("X-Goog-Upload-Command", "start")
      .WithHeaderParameter("X-Goog-Upload-Content-Type", "image/jpeg")
      .WithHeaderParameter("X-Goog-Upload-Protocol", "resumable")
      .WithHeaderParameter("X-Goog-Upload-Raw-Size", "7")
      .WithHeaderParameter("X-Goog-File-Name", "filename.jpg")
      .WithHeaderParameter("Authorization", _)
      .WillRespondWith(
          HttpResponse().WithHeaders({{"x-goog-upload-url", "upload-url"}}));

  ExpectHttp(mock.http(), "upload-url")
      .WithMethod("POST")
      .WithHeaderParameter("X-Goog-Upload-Command", "upload, finalize")
      .WithHeaderParameter("X-Goog-Upload-Offset", "0")
      .WithHeaderParameter("Authorization", _)
      .WithBody("content")
      .WillRespondWith("upload-token");

  ExpectHttp(mock.http(),
             "https://photoslibrary.googleapis.com/v1/mediaItems:batchCreate")
      .WithMethod("POST")
      .WithHeaderParameter("Content-Type", "application/json")
      .WithHeaderParameter("Authorization", _)
      .WithBody(IgnoringWhitespace(R"({
        "albumId": "album_id",
        "newMediaItems": [{
          "simpleMediaItem": {
            "uploadToken": "upload-token"
          }
        }]
      })"))
      .WillRespondWith(R"({
        "newMediaItemResults": [{
          "mediaItem": {
            "filename": "filename.jpg"
          }
        }]
      })");

  auto item = std::make_shared<Item>("item", "album_id", IItem::UnknownSize,
                                     IItem::UnknownTimeStamp,
                                     IItem::FileType::Directory);

  auto stream = std::make_shared<std::stringstream>();
  *stream << "content";
  ExpectImmediatePromise(
      provider->uploadFile(item, "filename.jpg",
                           provider->streamUploader(stream)),
      Pointee(AllOf(Property(&IItem::filename, "filename.jpg"))));
}

TEST(GooglePhotosTest, ReturnsAuthorizeLibraryUrl) {
  auto mock = CloudFactoryMock::create();
  EXPECT_THAT(
      mock.factory()->authorizationUrl("gphotos"),
      StrEq(
          R"(https://accounts.google.com/o/oauth2/auth?client_id=client_id&redirect_uri=http://redirect-uri/&scope=https://www.googleapis.com/auth/photoslibrary+openid%20email&response_type=code&access_type=offline&prompt=consent&state=state)"));
}

}  // namespace cloudstorage
