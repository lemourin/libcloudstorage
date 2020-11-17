#include "Utility/CloudFactoryMock.h"
#include "Utility/Item.h"

namespace cloudstorage {

using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;
using ::testing::SizeIs;
using ::testing::StrEq;

TEST(DropboxTest, GetsGeneralData) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("dropbox", {});

  ExpectHttp(mock.http(),
             "https://api.dropboxapi.com/2/users/get_current_account")
      .WithMethod("POST")
      .WithHeaderParameter("Content-Type", "")
      .WithHeaderParameter("Authorization", _)
      .WillRespondWith(R"js({ "email": "admin@admin.ru" })js");

  ExpectHttp(mock.http(), "https://api.dropboxapi.com/2/users/get_space_usage")
      .WithMethod("POST")
      .WithHeaderParameter("Content-Type", "")
      .WithHeaderParameter("Authorization", _)
      .WillRespondWith(
          R"js({ "allocation": { "allocated": 15 }, "used": 5 })js");

  ExpectImmediatePromise(provider->generalData(),
                         AllOf(Field(&GeneralData::username_, "admin@admin.ru"),
                               Field(&GeneralData::space_total_, 15),
                               Field(&GeneralData::space_used_, 5)));
}

TEST(DropboxTest, GetsItemData) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("dropbox", {});

  ExpectHttp(mock.http(), "https://api.dropboxapi.com/2/files/get_metadata")
      .WithMethod("POST")
      .WithHeaderParameter("Content-Type", "application/json")
      .WithHeaderParameter("Authorization", _)
      .WithBody(IgnoringWhitespace(R"js({ "path": "/path" })js"))
      .WillRespondWith(
          R"js({
                 "path_display": "/path",
                 "name": "path",
                 "size": 1234,
                 "client_modified": "2012-04-12T04:06:12.4213Z",
                 ".tag": "folder"
               })js");

  ExpectImmediatePromise(
      provider->getItemData("/path"),
      Pointee(AllOf(
          Property(&IItem::id, "/path"), Property(&IItem::filename, "path"),
          Property(&IItem::size, 1234),
          Property(&IItem::timestamp,
                   std::chrono::system_clock::from_time_t(1334203572)),
          Property(&IItem::type, IItem::FileType::Directory))));
}

TEST(DropboxTest, ListsDirectory) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("dropbox", {});

  ExpectHttp(mock.http(), "https://api.dropboxapi.com/2/files/list_folder")
      .WithMethod("POST")
      .WithHeaderParameter("Content-Type", "application/json")
      .WithHeaderParameter("Authorization", _)
      .WithBody(IgnoringWhitespace(R"js({ "path": "/path" })js"))
      .WillRespondWith(R"({
                            "entries": [{}],
                            "has_more": true,
                            "cursor": "next_token"
                          })");

  ExpectHttp(mock.http(),
             "https://api.dropboxapi.com/2/files/list_folder/continue")
      .WithMethod("POST")
      .WithBody(IgnoringWhitespace(R"js({ "cursor": "next_token" })js"))
      .WillRespondWith("{}");

  auto directory = std::make_shared<Item>(
      "filename", "/path", IItem::UnknownSize, IItem::UnknownTimeStamp,
      IItem::FileType::Directory);

  ExpectImmediatePromise(provider->listDirectory(directory), SizeIs(1));
}

TEST(DropboxTest, DownloadsItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("dropbox", {});

  ExpectHttp(mock.http(), "https://content.dropboxapi.com/2/files/download")
      .WithMethod("POST")
      .WithHeaderParameter("Authorization", _)
      .WithHeaderParameter("Content-Type", "")
      .WithHeaderParameter("Dropbox-API-arg",
                           IgnoringWhitespace(R"js({ "path": "/path" })js"))
      .WillRespondWith("content");

  auto item =
      std::make_shared<Item>("filename", "/path", IItem::UnknownSize,
                             IItem::UnknownTimeStamp, IItem::FileType::Unknown);

  auto stream = std::make_shared<std::stringstream>();
  ExpectImmediatePromise(provider->downloadFile(
      item, FullRange, provider->streamDownloader(stream)));
}

TEST(DropboxTest, DeletesItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("dropbox", {});

  ExpectHttp(mock.http(), "https://api.dropboxapi.com/2/files/delete")
      .WithMethod("POST")
      .WithHeaderParameter("Content-Type", "application/json")
      .WithHeaderParameter("Authorization", _)
      .WithBody(IgnoringWhitespace(R"js({ "path": "/path" })js"))
      .WillRespondWith("{}");

  auto item =
      std::make_shared<Item>("filename", "/path", IItem::UnknownSize,
                             IItem::UnknownTimeStamp, IItem::FileType::Unknown);

  ExpectImmediatePromise(provider->deleteItem(item));
}

TEST(DropboxTest, CreatesDirectory) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("dropbox", {});

  ExpectHttp(mock.http(), "https://api.dropboxapi.com/2/files/create_folder_v2")
      .WithMethod("POST")
      .WithHeaderParameter("Content-Type", "application/json")
      .WithHeaderParameter("Authorization", _)
      .WithBody(IgnoringWhitespace(R"js({ "path": "/path/directory" })js"))
      .WillRespondWith(R"js({ "metadata": { "name": "directory" } })js");

  auto parent = std::make_shared<Item>("parent", "/path", IItem::UnknownSize,
                                       IItem::UnknownTimeStamp,
                                       IItem::FileType::Directory);

  ExpectImmediatePromise(
      provider->createDirectory(parent, "directory"),
      Pointee(AllOf(Property(&IItem::filename, "directory"),
                    Property(&IItem::type, IItem::FileType::Directory))));
}

TEST(DropboxTest, MovesItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("dropbox", {});

  ExpectHttp(mock.http(), "https://api.dropboxapi.com/2/files/move_v2")
      .WithMethod("POST")
      .WithHeaderParameter("Content-Type", "application/json")
      .WithHeaderParameter("Authorization", _)
      .WithBody(IgnoringWhitespace(
          R"js({ "from_path": "/some/source", "to_path": "/path/source" })js"))
      .WillRespondWith(R"js({ "metadata": { "name": "filename" } })js");

  auto source =
      std::make_shared<Item>("source", "/some/source", IItem::UnknownSize,
                             IItem::UnknownTimeStamp, IItem::FileType::Image);

  auto destination = std::make_shared<Item>(
      "destination", "/path", IItem::UnknownSize, IItem::UnknownTimeStamp,
      IItem::FileType::Directory);

  ExpectImmediatePromise(
      provider->moveItem(source, destination),
      Pointee(AllOf(Property(&IItem::filename, "filename"),
                    Property(&IItem::type, IItem::FileType::Image))));
}

TEST(DropboxTest, RenamesItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("dropbox", {});

  ExpectHttp(mock.http(), "https://api.dropboxapi.com/2/files/move_v2")
      .WithMethod("POST")
      .WithHeaderParameter("Content-Type", "application/json")
      .WithHeaderParameter("Authorization", _)
      .WithBody(IgnoringWhitespace(R"js({
                                          "from_path": "/some/source",
                                          "to_path": "/some/new_name"
                                        })js"))
      .WillRespondWith(R"js({ "metadata": { "name": "new_name" } })js");

  auto source =
      std::make_shared<Item>("source", "/some/source", IItem::UnknownSize,
                             IItem::UnknownTimeStamp, IItem::FileType::Image);

  ExpectImmediatePromise(
      provider->renameItem(source, "new_name"),
      Pointee(AllOf(Property(&IItem::filename, "new_name"),
                    Property(&IItem::type, IItem::FileType::Image))));
}

TEST(DropboxTest, UploadsItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("dropbox", {});

  ExpectHttp(mock.http(),
             "https://content.dropboxapi.com/2/files/upload_session/start")
      .WithMethod("POST")
      .WithHeaderParameter("Authorization", _)
      .WithHeaderParameter("Content-Type", "application/octet-stream")
      .WithHeaderParameter("Dropbox-API-Arg", "null")
      .WithBody("content")
      .WillRespondWith(R"js({ "session_id": "ssid" })js");

  ExpectHttp(mock.http(),
             "https://content.dropboxapi.com/2/files/upload_session/finish")
      .WithMethod("POST")
      .WithHeaderParameter("Authorization", _)
      .WithHeaderParameter("Content-Type", "application/octet-stream")
      .WithHeaderParameter(
          "Dropbox-API-Arg",
          IgnoringWhitespace(
              R"js({
                     "commit": {
                       "mode": "overwrite",
                       "path": "/some/path/new_name"
                     },
                     "cursor": { "offset": 7, "session_id": "ssid" }
                   })js"))
      .WillRespondWith(R"js({ "name": "new_name" })js");

  auto parent = std::make_shared<Item>(
      "directory", "/some/path", IItem::UnknownSize, IItem::UnknownTimeStamp,
      IItem::FileType::Directory);

  auto stream = std::make_shared<std::stringstream>();
  *stream << "content";
  ExpectImmediatePromise(provider->uploadFile(parent, "new_name",
                                              provider->streamUploader(stream)),
                         Pointee(Property(&IItem::filename, "new_name")));
}

TEST(DropboxTest, GetsThumbnail) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("dropbox", {});

  ExpectHttp(mock.http(),
             "https://content.dropboxapi.com/2/files/get_thumbnail")
      .WithMethod("POST")
      .WithHeaderParameter("Content-Type", "")
      .WithHeaderParameter("Authorization", _)
      .WithHeaderParameter(
          "Dropbox-API-arg",
          IgnoringWhitespace(R"js({ "path": "/some/source" })js"))
      .WillRespondWith("thumbnail");

  auto source =
      std::make_shared<Item>("source.jpg", "/some/source", IItem::UnknownSize,
                             IItem::UnknownTimeStamp, IItem::FileType::Image);

  auto stream = std::make_shared<std::stringstream>();
  ExpectImmediatePromise(
      provider->generateThumbnail(source, provider->streamDownloader(stream)));

  EXPECT_EQ(stream->str(), "thumbnail");
}

TEST(DropboxTest, GetsItemUrl) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("dropbox", {});

  ExpectHttp(mock.http(),
             "https://api.dropboxapi.com/2/files/get_temporary_link")
      .WithMethod("POST")
      .WithHeaderParameter("Content-Type", "application/json")
      .WithHeaderParameter("Authorization", _)
      .WithBody(IgnoringWhitespace(R"js({ "path": "/some_path" })js"))
      .WillRespondWith(R"js({ "link": "item-link" })js");

  ExpectHttp(mock.http(), "item-link")
      .WithMethod("HEAD")
      .WillRespondWithCode(200);

  auto item =
      std::make_shared<Item>("item", "/some_path", IItem::UnknownSize,
                             IItem::UnknownTimeStamp, IItem::FileType::Unknown);

  ExpectImmediatePromise(provider->getFileUrl(item), StrEq("item-link"));
}

TEST(DropboxTest, ExchangesAuthorizationCode) {
  auto mock = CloudFactoryMock::create();
  ICloudFactory::ProviderInitData data;
  data.hints_["client_id"] = "client_id";
  data.hints_["client_secret"] = "client_secret";
  data.hints_["redirect_uri"] = "http://redirect-uri/";

  ExpectHttp(mock.http(), "https://api.dropboxapi.com/oauth2/token")
      .WithMethod("POST")
      .WithParameter("grant_type", "authorization_code")
      .WithParameter("client_id", "client_id")
      .WithParameter("client_secret", "client_secret")
      .WithParameter("redirect_uri", "http://redirect-uri/")
      .WithParameter("code", "code")
      .WillRespondWith(R"js({
                              "refresh_token": "token",
                              "access_token": "access_token",
                              "expires_in": 0
                            })js");

  ExpectImmediatePromise(
      mock.factory()->exchangeAuthorizationCode("dropbox", data, "code"),
      AllOf(Field(&Token::token_, "access_token"),
            Field(&Token::access_token_, "access_token")));
}

TEST(DropboxTest, RootHasEmptyId) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("dropbox", {});

  EXPECT_TRUE(provider->root()->id().empty());
}

TEST(DropboxTest, GetsAuthorizeLibraryUrl) {
  auto mock = CloudFactoryMock::create();
  EXPECT_EQ(
      mock.factory()->authorizationUrl("dropbox"),
      R"(https://www.dropbox.com/oauth2/authorize?response_type=code&client_id=client_id&redirect_uri=http://redirect-uri/&state=state)");
}

}  // namespace cloudstorage
