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

  auto current_account_response =
      MockResponse(R"js({ "email": "admin@admin.ru" })js");
  EXPECT_CALL(*current_account_response,
              setHeaderParameter("Content-Type", ""));
  EXPECT_CALL(*current_account_response,
              setHeaderParameter("Authorization", _));

  EXPECT_CALL(*mock.http(),
              create("https://api.dropboxapi.com/2/users/get_current_account",
                     "POST", true))
      .WillOnce(Return(current_account_response));

  auto space_usage_response =
      MockResponse(R"js({ "allocation": { "allocated": 15 }, "used": 5 })js");
  EXPECT_CALL(*space_usage_response, setHeaderParameter("Content-Type", ""));
  EXPECT_CALL(*space_usage_response, setHeaderParameter("Authorization", _));

  EXPECT_CALL(*mock.http(),
              create("https://api.dropboxapi.com/2/users/get_space_usage",
                     "POST", true))
      .WillOnce(Return(space_usage_response));

  ExpectImmediatePromise(provider->generalData(),
                         AllOf(Field(&GeneralData::username_, "admin@admin.ru"),
                               Field(&GeneralData::space_total_, 15),
                               Field(&GeneralData::space_used_, 5)));
}

TEST(DropboxTest, GetsItemData) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("dropbox", {});

  auto response = MockResponse(
      R"js({
             "path_display": "/path",
             "name": "path",
             "size": 1234,
             "client_modified": "2012-04-12T04:06:12.4213Z",
             ".tag": "folder"
           })js", IgnoringWhitespace(R"js({ "path": "/path" })js"));
  EXPECT_CALL(*response, setHeaderParameter("Authorization", _));
  EXPECT_CALL(*response,
              setHeaderParameter("Content-Type", "application/json"));

  EXPECT_CALL(
      *mock.http(),
      create("https://api.dropboxapi.com/2/files/get_metadata", "POST", true))
      .WillOnce(Return(response));

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

  auto first_page_response = MockResponse(
      R"js({
             "entries":
             [ {}], "has_more": true, "cursor": "next_token"
           })js", IgnoringWhitespace(R"js({ "path": "/path" })js"));
  EXPECT_CALL(*first_page_response,
              setHeaderParameter("Content-Type", "application/json"));
  EXPECT_CALL(*first_page_response, setHeaderParameter("Authorization", _));

  EXPECT_CALL(
      *mock.http(),
      create("https://api.dropboxapi.com/2/files/list_folder", "POST", true))
      .WillOnce(Return(first_page_response));

  auto second_page_repsonse = MockResponse(
      R"js({})js", IgnoringWhitespace(R"js({ "cursor": "next_token" })js"));

  EXPECT_CALL(*mock.http(),
              create("https://api.dropboxapi.com/2/files/list_folder/continue",
                     "POST", true))
      .WillOnce(Return(second_page_repsonse));

  auto directory = std::make_shared<Item>(
      "filename", "/path", IItem::UnknownSize, IItem::UnknownTimeStamp,
      IItem::FileType::Directory);

  ExpectImmediatePromise(provider->listDirectory(directory), SizeIs(1));
}

TEST(DropboxTest, DownloadsItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("dropbox", {});

  auto response = MockResponse("content");
  EXPECT_CALL(*response, setHeaderParameter("Authorization", _));
  EXPECT_CALL(*response, setHeaderParameter("Content-Type", ""));
  EXPECT_CALL(*response, setHeaderParameter(
                             "Dropbox-API-arg",
                             IgnoringWhitespace(R"js({ "path": "/path" })js")));

  EXPECT_CALL(
      *mock.http(),
      create("https://content.dropboxapi.com/2/files/download", "POST", true))
      .WillOnce(Return(response));

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

  auto response = MockResponse(
      R"js({})js", IgnoringWhitespace(R"js({ "path": "/path" })js"));
  EXPECT_CALL(*response, setHeaderParameter("Authorization", _));
  EXPECT_CALL(*response,
              setHeaderParameter("Content-Type", "application/json"));

  EXPECT_CALL(*mock.http(),
              create("https://api.dropboxapi.com/2/files/delete", "POST", true))
      .WillOnce(Return(response));

  auto item =
      std::make_shared<Item>("filename", "/path", IItem::UnknownSize,
                             IItem::UnknownTimeStamp, IItem::FileType::Unknown);

  ExpectImmediatePromise(provider->deleteItem(item));
}

TEST(DropboxTest, CreatesDirectory) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("dropbox", {});

  auto response =
      MockResponse(R"js({ "metadata": { "name": "directory" } })js",
                   IgnoringWhitespace(R"js({ "path": "/path/directory" })js"));
  EXPECT_CALL(*response, setHeaderParameter("Authorization", _));
  EXPECT_CALL(*response,
              setHeaderParameter("Content-Type", "application/json"));
  EXPECT_CALL(*mock.http(),
              create("https://api.dropboxapi.com/2/files/create_folder_v2",
                     "POST", true))
      .WillOnce(Return(response));

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

  auto response = MockResponse(
      R"js({ "metadata": { "name": "filename" } })js",
      IgnoringWhitespace(
          R"js({ "from_path": "/some/source", "to_path": "/path/source" })js"));
  EXPECT_CALL(*response, setHeaderParameter("Authorization", _));
  EXPECT_CALL(*response,
              setHeaderParameter("Content-Type", "application/json"));
  EXPECT_CALL(*mock.http(), create("https://api.dropboxapi.com/2/files/move_v2",
                                   "POST", true))
      .WillOnce(Return(response));

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

  auto response =
      MockResponse(R"js({ "metadata": { "name": "new_name" } })js",
                   IgnoringWhitespace(R"js({
                                             "from_path": "/some/source",
                                             "to_path": "/some/new_name"
                                           })js"));
  EXPECT_CALL(*response, setHeaderParameter("Authorization", _));
  EXPECT_CALL(*response,
              setHeaderParameter("Content-Type", "application/json"));
  EXPECT_CALL(*mock.http(), create("https://api.dropboxapi.com/2/files/move_v2",
                                   "POST", true))
      .WillOnce(Return(response));

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

  auto start_session_response =
      MockResponse(R"js({ "session_id": "ssid" })js", "content");
  EXPECT_CALL(*start_session_response, setHeaderParameter("Authorization", _));
  EXPECT_CALL(*start_session_response,
              setHeaderParameter("Content-Type", "application/octet-stream"));
  EXPECT_CALL(*start_session_response,
              setHeaderParameter("Dropbox-API-Arg", "null"));
  EXPECT_CALL(
      *mock.http(),
      create("https://content.dropboxapi.com/2/files/upload_session/start",
             "POST", true))
      .WillOnce(Return(start_session_response));

  auto finish_session_response = MockResponse(R"js({ "name": "new_name" })js");
  EXPECT_CALL(*finish_session_response, setHeaderParameter("Authorization", _));
  EXPECT_CALL(*finish_session_response,
              setHeaderParameter("Content-Type", "application/octet-stream"));
  EXPECT_CALL(*finish_session_response,
              setHeaderParameter(
                  "Dropbox-API-Arg",
                  IgnoringWhitespace(
                      R"js({
                             "commit": {
                               "mode": "overwrite",
                               "path": "/some/path/new_name"
                             },
                             "cursor": { "offset": 7, "session_id": "ssid" }
                           })js")));
  EXPECT_CALL(
      *mock.http(),
      create("https://content.dropboxapi.com/2/files/upload_session/finish",
             "POST", true))
      .WillOnce(Return(finish_session_response));

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

  auto response = MockResponse("thumbnail");
  EXPECT_CALL(*response, setHeaderParameter("Authorization", _));
  EXPECT_CALL(*response, setHeaderParameter("Content-Type", ""));
  EXPECT_CALL(*response,
              setHeaderParameter(
                  "Dropbox-API-arg",
                  IgnoringWhitespace(R"js({ "path": "/some/source" })js")));

  EXPECT_CALL(*mock.http(),
              create("https://content.dropboxapi.com/2/files/get_thumbnail",
                     "POST", true))
      .WillOnce(Return(response));

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

  auto response =
      MockResponse(R"js({ "link": "item-link" })js",
                   IgnoringWhitespace(R"js({ "path": "/some_path" })js"));
  EXPECT_CALL(*response, setHeaderParameter("Authorization", _))
      .WillRepeatedly(Return());
  EXPECT_CALL(*response, setHeaderParameter("Content-Type", "application/json"))
      .WillRepeatedly(Return());

  EXPECT_CALL(*mock.http(),
              create("https://api.dropboxapi.com/2/files/get_temporary_link",
                     "POST", true))
      .WillRepeatedly(Return(response));
  EXPECT_CALL(*mock.http(), create("item-link", "HEAD", true))
      .WillOnce(Return(MockResponse(200)));

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

  auto response = MockResponse(R"js({
                                      "refresh_token": "token",
                                      "access_token": "access_token",
                                      "expires_in": 0
                                    })js");
  EXPECT_CALL(*response, setParameter("grant_type", "authorization_code"));
  EXPECT_CALL(*response, setParameter("client_id", "client_id"));
  EXPECT_CALL(*response, setParameter("client_secret", "client_secret"));
  EXPECT_CALL(*response, setParameter("redirect_uri", "http://redirect-uri/"));
  EXPECT_CALL(*response, setParameter("code", "code"));

  EXPECT_CALL(*mock.http(),
              create("https://api.dropboxapi.com/oauth2/token", "POST", true))
      .WillOnce(Return(MockResponse(200)))
      .WillOnce(Return(response));

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
  ICloudFactory::ProviderInitData data;
  data.hints_["client_id"] = "client_id";
  data.hints_["client_secret"] = "client_secret";
  data.hints_["redirect_uri"] = "http://redirect-uri/";
  data.hints_["state"] = "state";

  EXPECT_EQ(
      mock.factory()->authorizationUrl("dropbox", data),
      R"(https://www.dropbox.com/oauth2/authorize?response_type=code&client_id=client_id&redirect_uri=http://redirect-uri/&state=state)");
}

}  // namespace cloudstorage
