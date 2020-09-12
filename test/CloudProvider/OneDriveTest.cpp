#include "Utility/CloudFactoryMock.h"
#include "Utility/Item.h"

namespace cloudstorage {

using ::testing::_;
using ::testing::AllOf;
using ::testing::Field;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;
using ::testing::SizeIs;

TEST(OneDriveTest, GetsGeneralData) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("onedrive", {});

  EXPECT_CALL(*mock.http(), create("/me/drive", "GET", true))
      .WillOnce(Return(
          MockResponse(R"js({ "quota": { "total": 10, "used": 5 } })js")));
  EXPECT_CALL(*mock.http(), create("/me", "GET", true))
      .WillOnce(Return(
          MockResponse(R"js({ "userPrincipalName": "admin@admin.ru" })js")));

  ExpectImmediatePromise(provider->generalData(),
                         AllOf(Field(&GeneralData::username_, "admin@admin.ru"),
                               Field(&GeneralData::space_total_, 10),
                               Field(&GeneralData::space_used_, 5)));
}

TEST(OneDriveTest, GetsItemData) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("onedrive", {});

  auto response =
      MockResponse(R"js({
                          "name": "filename",
                          "id": "id",
                          "size": 1234,
                          "lastModifiedDateTime": "2012-04-12T04:06:12.4213Z"
                        })js");
  EXPECT_CALL(
      *response,
      setParameter("select",
                   "name,folder,audio,image,photo,video,id,size,"
                   "lastModifiedDateTime,thumbnails,@content.downloadUrl"));
  EXPECT_CALL(*response, setParameter("expand", "thumbnails"));

  EXPECT_CALL(*mock.http(), create("/drive/items/id", "GET", true))
      .WillOnce(Return(response));

  ExpectImmediatePromise(
      provider->getItemData("id"),
      Pointee(
          AllOf(Property(&IItem::filename, "filename"),
                Property(&IItem::id, "id"), Property(&IItem::size, 1234),
                Property(&IItem::timestamp,
                         std::chrono::system_clock::from_time_t(1334203572)))));
}

TEST(OneDriveTest, ListsDirectory) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("onedrive", {});

  auto response = MockResponse(R"js({
                                      "value":
                                      [ {}]
                                    })js");
  EXPECT_CALL(
      *response,
      setParameter("select",
                   "name,folder,audio,image,photo,video,id,size,"
                   "lastModifiedDateTime,thumbnails,@content.downloadUrl"));
  EXPECT_CALL(*response, setParameter("expand", "thumbnails"));

  EXPECT_CALL(*mock.http(), create("/drive/items/id/children", "GET", true))
      .WillOnce(Return(response));

  ExpectImmediatePromise(
      provider->listDirectory(std::make_unique<Item>(
          "directory", "id", IItem::UnknownSize, IItem::UnknownTimeStamp,
          IItem::FileType::Directory)),
      SizeIs(1));
}

TEST(OneDriveTest, ListsDirectoryPage) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("onedrive", {});

  auto response =
      MockResponse(R"js({ "value": [], "@odata.nextLink": "/next_token" })js");
  EXPECT_CALL(*mock.http(), create("/page_token", "GET", true))
      .WillOnce(Return(response));

  ExpectImmediatePromise(
      provider->listDirectoryPage(
          std::make_unique<Item>("directory", "id", IItem::UnknownSize,
                                 IItem::UnknownTimeStamp,
                                 IItem::FileType::Directory),
          "/page_token"),
      AllOf(Field(&PageData::next_token_, "/next_token")));
}

TEST(OneDriveTest, DeletesItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("onedrive", {});

  EXPECT_CALL(*mock.http(), create("/drive/items/id", "DELETE", true))
      .WillOnce(Return(MockResponse("")));

  ExpectImmediatePromise(provider->deleteItem(std::make_unique<Item>(
      "directory", "id", IItem::UnknownSize, IItem::UnknownTimeStamp,
      IItem::FileType::Directory)));
}

TEST(OneDriveTest, CreatesDirectory) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("onedrive", {});

  auto response = MockResponse(
      R"js({ "name": "new_directory" })js",
      IgnoringWhitespace(R"js({ "folder": {}, "name": "new_directory" })js"));
  EXPECT_CALL(*response,
              setHeaderParameter("Content-Type", "application/json"));
  EXPECT_CALL(*response, setHeaderParameter("Authorization", _));

  EXPECT_CALL(*mock.http(), create("/drive/items/id/children", "POST", true))
      .WillOnce(Return(response));

  ExpectImmediatePromise(
      provider->createDirectory(
          std::make_unique<Item>("directory", "id", IItem::UnknownSize,
                                 IItem::UnknownTimeStamp,
                                 IItem::FileType::Directory),
          "new_directory"),
      Pointee(Property(&IItem::filename, "new_directory")));
}

TEST(OneDriveTest, DownloadsItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("onedrive", {});

  EXPECT_CALL(*mock.http(), create("/drive/items/id/content", "GET", true))
      .WillOnce(Return(MockResponse("content")));

  auto stream = std::make_shared<std::stringstream>();
  ExpectImmediatePromise(provider->downloadFile(
      std::make_unique<Item>("file", "id", IItem::UnknownSize,
                             IItem::UnknownTimeStamp, IItem::FileType::Unknown),
      FullRange, provider->streamDownloader(stream)));

  EXPECT_EQ(stream->str(), "content");
}

TEST(OneDriveTest, MovesItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("onedrive", {});

  auto response = MockResponse(
      R"js({ "id": "source_id" })js",
      IgnoringWhitespace(
          R"js({ "parentReference": { "id": "destination_id" } })js"));
  EXPECT_CALL(*response,
              setHeaderParameter("Content-Type", "application/json"));
  EXPECT_CALL(*response, setHeaderParameter("Authorization", _));

  EXPECT_CALL(*mock.http(), create("/drive/items/source_id", "PATCH", true))
      .WillOnce(Return(response));

  auto source =
      std::make_shared<Item>("source", "source_id", IItem::UnknownSize,
                             IItem::UnknownTimeStamp, IItem::FileType::Unknown);
  auto destination = std::make_shared<Item>(
      "destination", "destination_id", IItem::UnknownSize,
      IItem::UnknownTimeStamp, IItem::FileType::Directory);

  ExpectImmediatePromise(provider->moveItem(source, destination),
                         Pointee(Property(&IItem::id, "source_id")));
}

TEST(OneDriveTest, MovesItemToRootDirectory) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("onedrive", {});

  auto response = MockResponse(
      R"js({ "id": "source_id" })js",
      IgnoringWhitespace(
          R"js({ "parentReference": { "path": "/drive/root" } })js"));
  EXPECT_CALL(*response,
              setHeaderParameter("Content-Type", "application/json"));
  EXPECT_CALL(*response, setHeaderParameter("Authorization", _));

  EXPECT_CALL(*mock.http(), create("/drive/items/source_id", "PATCH", true))
      .WillOnce(Return(response));

  auto source =
      std::make_shared<Item>("source", "source_id", IItem::UnknownSize,
                             IItem::UnknownTimeStamp, IItem::FileType::Unknown);

  ExpectImmediatePromise(provider->moveItem(source, provider->root()),
                         Pointee(Property(&IItem::id, "source_id")));
}

TEST(OneDriveTest, RenamesItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("onedrive", {});

  auto response =
      MockResponse(R"js({ "name": "new_name" })js",
                   IgnoringWhitespace(R"js({ "name": "new_name" })js"));
  EXPECT_CALL(*response,
              setHeaderParameter("Content-Type", "application/json"));
  EXPECT_CALL(*response, setHeaderParameter("Authorization", _));

  EXPECT_CALL(*mock.http(), create("/drive/items/source_id", "PATCH", true))
      .WillOnce(Return(response));

  auto item =
      std::make_shared<Item>("source", "source_id", IItem::UnknownSize,
                             IItem::UnknownTimeStamp, IItem::FileType::Unknown);

  ExpectImmediatePromise(provider->renameItem(item, "new_name"),
                         Pointee(Property(&IItem::filename, "new_name")));
}

TEST(OneDriveTest, UploadsItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("onedrive", {});

  EXPECT_CALL(*mock.http(),
              create("/me/drive/items/parent_id:/filename:/createUploadSession",
                     "POST", true))
      .WillOnce(
          Return(MockResponse(R"js({ "uploadUrl": "http://upload-url/" })js")));

  auto response = MockResponse(R"js({ "name": "filename" })js", "content");
  EXPECT_CALL(*response, setHeaderParameter("Content-Range", "bytes 0-6/7"));
  EXPECT_CALL(*response, setHeaderParameter("Authorization", _));

  EXPECT_CALL(*mock.http(), create("http://upload-url/", "PUT", true))
      .WillOnce(Return(response));

  auto parent = std::make_shared<Item>(
      "directory", "parent_id", IItem::UnknownSize, IItem::UnknownTimeStamp,
      IItem::FileType::Directory);

  auto stream = std::make_shared<std::stringstream>();
  *stream << "content";
  ExpectImmediatePromise(provider->uploadFile(parent, "filename",
                                              provider->streamUploader(stream)),
                         Pointee(Property(&IItem::filename, "filename")));
}

TEST(OneDriveTest, AuthorizationTest) {
  auto mock = CloudFactoryMock::create();

  ICloudFactory::ProviderInitData data;
  data.hints_["client_id"] = "client_id";
  data.hints_["client_secret"] = "client_secret";

  auto provider = mock.factory()->create("onedrive", std::move(data));

  EXPECT_CALL(*mock.http(), create("/me/drive", "GET", true))
      .WillOnce(Return(MockResponse(401)));
  EXPECT_CALL(
      *mock.http(),
      create("https://login.microsoftonline.com/common/oauth2/v2.0/token",
             "POST", true))
      .WillOnce(Return(MockResponse(
          200, R"js({ "access_token": "token" })js",
          R"(client_id=client_id&client_secret=client_secret&refresh_token=&grant_type=refresh_token)")));
  EXPECT_CALL(*mock.http(),
              create("https://graph.microsoft.com/v1.0/me", "GET", true))
      .WillOnce(
          Return(MockResponse(R"js({ "mySite": "http://example.com/" })js")));
  EXPECT_CALL(*mock.http(),
              create("http://example.com/_api/v2.0/me/drive", "GET", true))
      .WillOnce(Return(MockResponse(R"js({})js")));
  EXPECT_CALL(*mock.http(),
              create("http://example.com/_api/v2.0/me", "GET", true))
      .WillOnce(Return(MockResponse(R"js({})js")));

  ExpectImmediatePromise(provider->generalData(), _);

  EXPECT_EQ(provider->hints()["endpoint"], "http://example.com/_api/v2.0");
}

TEST(OneDriveTest, ExchangesAuthorizationCode) {
  auto mock = CloudFactoryMock::create();
  ICloudFactory::ProviderInitData data;
  data.hints_["client_id"] = "client_id";
  data.hints_["client_secret"] = "client_secret";
  data.hints_["redirect_uri"] = "http://redirect-uri/";

  EXPECT_CALL(
      *mock.http(),
      create("https://login.microsoftonline.com/common/oauth2/v2.0/token",
             "POST", true))
      .WillRepeatedly(Return(MockResponse(
          R"js({
                 "refresh_token": "token",
                 "access_token": "access_token",
                 "expires_in": 0
               })js",
          R"(client_id=client_id&client_secret=client_secret&redirect_uri=http%3A%2F%2Fredirect-uri%2F&code=code&grant_type=authorization_code)")));

  ExpectImmediatePromise(
      mock.factory()->exchangeAuthorizationCode("onedrive", data, "code"),
      AllOf(Field(&Token::token_, "token"),
            Field(&Token::access_token_, "access_token")));
}

TEST(OneDriveTest, GetsAuthorizeLibraryUrl) {
  auto mock = CloudFactoryMock::create();
  ICloudFactory::ProviderInitData data;
  data.hints_["client_id"] = "client_id";
  data.hints_["client_secret"] = "client_secret";
  data.hints_["redirect_uri"] = "http://redirect-uri/";
  data.hints_["state"] = "state";

  EXPECT_EQ(
      mock.factory()->authorizationUrl("onedrive", data),
      R"(https://login.microsoftonline.com/common/oauth2/v2.0/authorize?client_id=client_id&scope=offline_access%20user.read%20files.readwrite&response_type=code&redirect_uri=http%3A%2F%2Fredirect-uri%2F&state=state)");
}

}  // namespace cloudstorage
