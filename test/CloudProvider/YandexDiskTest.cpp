#include "Utility/CloudFactoryMock.h"
#include "Utility/Item.h"
#include "Utility/Utility.h"

namespace cloudstorage {

using ::testing::_;
using ::testing::Field;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;
using ::testing::SizeIs;
using ::testing::StrEq;

TEST(YandexDiskTest, GetsGeneralData) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("yandex", {});

  EXPECT_CALL(*mock.http(), create("https://login.yandex.ru/info", "GET", true))
      .WillOnce(Return(MockResponse(R"js({ "login": "admin@admin.ru" })js")));
  EXPECT_CALL(*mock.http(),
              create("https://cloud-api.yandex.net/v1/disk", "GET", true))
      .WillOnce(Return(
          MockResponse(R"js({ "total_space": 10, "used_space": 5 })js")));

  ExpectImmediatePromise(provider->generalData(),
                         AllOf(Field(&GeneralData::username_, "admin@admin.ru"),
                               Field(&GeneralData::space_total_, 10),
                               Field(&GeneralData::space_used_, 5)));
}

TEST(YandexDiskTest, GetsItemDataForItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("yandex", {});

  auto response = MockResponse(R"js({
                                      "name": "filename",
                                      "path": "id",
                                      "size": 1234,
                                      "mime_type": "image/jpeg",
                                      "modified": "2012-04-12T04:06:12.4213Z"
                                    })js");
  EXPECT_CALL(*response, setParameter("path", "id"));

  EXPECT_CALL(
      *mock.http(),
      create("https://cloud-api.yandex.net/v1/disk/resources", "GET", true))
      .WillOnce(Return(response));

  ExpectImmediatePromise(
      provider->getItemData("id"),
      Pointee(
          AllOf(Property(&IItem::filename, "filename"),
                Property(&IItem::id, "id"), Property(&IItem::size, 1234),
                Property(&IItem::type, IItem::FileType::Image),
                Property(&IItem::timestamp,
                         std::chrono::system_clock::from_time_t(1334203572)))));
}

TEST(YandexDiskTest, GetsItemDataForDirectory) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("yandex", {});

  auto response = MockResponse(R"js({
                                      "name": "filename",
                                      "path": "id",
                                      "size": 1234,
                                      "type": "dir",
                                      "modified": "2012-04-12T04:06:12.4213Z"
                                    })js");
  EXPECT_CALL(*response, setParameter("path", "id"));

  EXPECT_CALL(
      *mock.http(),
      create("https://cloud-api.yandex.net/v1/disk/resources", "GET", true))
      .WillOnce(Return(response));

  ExpectImmediatePromise(
      provider->getItemData("id"),
      Pointee(
          AllOf(Property(&IItem::filename, "filename"),
                Property(&IItem::id, "id"), Property(&IItem::size, 1234),
                Property(&IItem::type, IItem::FileType::Directory),
                Property(&IItem::timestamp,
                         std::chrono::system_clock::from_time_t(1334203572)))));
}

TEST(YandexDiskTest, ListsDirectory) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("yandex", {});

  auto first_page_response = MockResponse(
      R"({
           "_embedded": {
             "items": [{}],
             "offset": 0,
             "limit": 1,
             "total": 5
           }
         })");
  EXPECT_CALL(*first_page_response, setParameter("path", "id"));

  auto second_page_response = MockResponse(R"({
           "_embedded": {
             "items": [{}, {}, {}, {}],
             "offset": 1,
             "limit": 4,
             "total": 5
           }
         })");
  EXPECT_CALL(*second_page_response, setParameter("path", "id"));
  EXPECT_CALL(*second_page_response, setParameter("offset", "1"));

  EXPECT_CALL(
      *mock.http(),
      create("https://cloud-api.yandex.net/v1/disk/resources", "GET", true))
      .WillOnce(Return(first_page_response))
      .WillOnce(Return(second_page_response));

  auto directory = std::make_shared<Item>("filename", "id", IItem::UnknownSize,
                                          IItem::UnknownTimeStamp,
                                          IItem::FileType::Directory);

  ExpectImmediatePromise(provider->listDirectory(directory), SizeIs(5));
}

TEST(YandexDiskTest, DeletesItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("yandex", {});

  auto response = MockResponse(200);
  EXPECT_CALL(*response, setParameter("path", "file_id"));
  EXPECT_CALL(*response, setParameter("permamently", "true"));
  EXPECT_CALL(
      *mock.http(),
      create("https://cloud-api.yandex.net/v1/disk/resources", "DELETE", true))
      .WillOnce(Return(response));

  auto item =
      std::make_shared<Item>("filename", "file_id", IItem::UnknownSize,
                             IItem::UnknownTimeStamp, IItem::FileType::Unknown);

  ExpectImmediatePromise(provider->deleteItem(item));
}

TEST(YandexDiskTest, DeletesFolder) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("yandex", {});

  auto response = MockResponse(202, R"js({ "href": "status-url" })js");
  EXPECT_CALL(*response, setParameter("path", "directory_id"));
  EXPECT_CALL(*response, setParameter("permamently", "true"));
  EXPECT_CALL(
      *mock.http(),
      create("https://cloud-api.yandex.net/v1/disk/resources", "DELETE", true))
      .WillOnce(Return(response));

  EXPECT_CALL(*mock.http(), create("status-url", "GET", true))
      .WillOnce(Return(MockResponse(R"js({ "status": "in-progress" })js")))
      .WillOnce(Return(MockResponse(R"js({ "status": "in-progress" })js")))
      .WillOnce(Return(MockResponse(R"js({ "status": "success" })js")));

  auto item = std::make_shared<Item>(
      "filename", "directory_id", IItem::UnknownSize, IItem::UnknownTimeStamp,
      IItem::FileType::Directory);

  ExpectImmediatePromise(provider->deleteItem(item));
}

TEST(YandexDiskTest, HandlesDeleteFailure) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("yandex", {});

  auto response = MockResponse(202, R"js({ "href": "status-url" })js");
  EXPECT_CALL(*response, setParameter("path", "directory_id"));
  EXPECT_CALL(*response, setParameter("permamently", "true"));
  EXPECT_CALL(
      *mock.http(),
      create("https://cloud-api.yandex.net/v1/disk/resources", "DELETE", true))
      .WillOnce(Return(response));

  EXPECT_CALL(*mock.http(), create("status-url", "GET", true))
      .WillOnce(Return(MockResponse(R"js({ "status": "in-progress" })js")))
      .WillOnce(Return(MockResponse(R"js({ "status": "in-progress" })js")))
      .WillOnce(Return(MockResponse(R"js({ "status": "failed" })js")));

  auto item = std::make_shared<Item>(
      "filename", "directory_id", IItem::UnknownSize, IItem::UnknownTimeStamp,
      IItem::FileType::Directory);

  ExpectFailedPromise(provider->deleteItem(item),
                      Field(&Error::code_, IHttpRequest::Failure));
}

TEST(YandexDiskTest, CreatesDirectory) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("yandex", {});

  auto response = MockResponse(200);
  EXPECT_CALL(*response, setParameter("path", "folder_id/directory_name"));
  EXPECT_CALL(
      *mock.http(),
      create("https://cloud-api.yandex.net/v1/disk/resources/", "PUT", true))
      .WillOnce(Return(response));

  auto parent = std::make_shared<Item>(
      "parent", "folder_id", IItem::UnknownSize, IItem::UnknownTimeStamp,
      IItem::FileType::Directory);

  ExpectImmediatePromise(
      provider->createDirectory(parent, "directory_name"),
      Pointee(AllOf(Property(&IItem::filename, "directory_name"),
                    Property(&IItem::size, 0),
                    Property(&IItem::id, "folder_id/directory_name"),
                    Property(&IItem::type, IItem::FileType::Directory))));
}

TEST(YandexDiskTest, MovesItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("yandex", {});

  auto response = MockResponse(R"({
    "name": "filename",
    "path": "id"
  })");
  EXPECT_CALL(*response, setParameter("from", "source_id"));
  EXPECT_CALL(*response,
              setParameter("path", "destination_id/source_filename"));
  EXPECT_CALL(*mock.http(),
              create("https://cloud-api.yandex.net/v1/disk/resources/move",
                     "POST", true))
      .WillOnce(Return(response));

  auto source =
      std::make_shared<Item>("source_filename", "source_id", IItem::UnknownSize,
                             IItem::UnknownTimeStamp, IItem::FileType::Unknown);

  auto destination = std::make_shared<Item>(
      "destination_filename", "destination_id", IItem::UnknownSize,
      IItem::UnknownTimeStamp, IItem::FileType::Directory);

  ExpectImmediatePromise(
      provider->moveItem(source, destination),
      Pointee(AllOf(Property(&IItem::filename, "filename"),
                    Property(&IItem::id, "id"),
                    Property(&IItem::type, IItem::FileType::Unknown))));
}

TEST(YandexDiskTest, MovesFolder) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("yandex", {});

  auto response = MockResponse(202, R"js({ "href": "status-url" })js");
  EXPECT_CALL(*response, setParameter("from", "source_id"));
  EXPECT_CALL(*response,
              setParameter("path", "destination_id/source_filename"));
  EXPECT_CALL(*mock.http(),
              create("https://cloud-api.yandex.net/v1/disk/resources/move",
                     "POST", true))
      .WillOnce(Return(response));

  EXPECT_CALL(*mock.http(), create("status-url", "GET", true))
      .WillOnce(Return(MockResponse(R"js({ "status": "in-progress" })js")))
      .WillOnce(Return(MockResponse(R"js({ "status": "in-progress" })js")))
      .WillOnce(Return(MockResponse(R"js({ "status": "success" })js")));

  auto source = std::make_shared<Item>(
      "source_filename", "source_id", IItem::UnknownSize,
      IItem::UnknownTimeStamp, IItem::FileType::Directory);

  auto destination = std::make_shared<Item>(
      "destination_filename", "destination_id", IItem::UnknownSize,
      IItem::UnknownTimeStamp, IItem::FileType::Directory);

  ExpectImmediatePromise(
      provider->moveItem(source, destination),
      Pointee(AllOf(Property(&IItem::filename, "source_filename"),
                    Property(&IItem::id, "destination_id/source_filename"),
                    Property(&IItem::type, IItem::FileType::Directory))));
}

TEST(YandexDiskTest, RenamesItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("yandex", {});

  auto response = MockResponse(R"({
    "name": "filename",
    "path": "id"
  })");
  EXPECT_CALL(*response, setParameter("from", "source_id"));
  EXPECT_CALL(*response, setParameter("path", "source_id/new_name"));
  EXPECT_CALL(*mock.http(),
              create("https://cloud-api.yandex.net/v1/disk/resources/move",
                     "POST", true))
      .WillOnce(Return(response));

  auto item =
      std::make_shared<Item>("source_filename", "source_id", IItem::UnknownSize,
                             IItem::UnknownTimeStamp, IItem::FileType::Unknown);

  ExpectImmediatePromise(
      provider->renameItem(item, "new_name"),
      Pointee(AllOf(Property(&IItem::filename, "filename"),
                    Property(&IItem::id, "id"),
                    Property(&IItem::type, IItem::FileType::Unknown))));
}

TEST(YandexDiskTest, RenamesFolder) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("yandex", {});

  auto response = MockResponse(202, R"js({ "href": "status-url" })js");
  EXPECT_CALL(*response, setParameter("from", "source_id"));
  EXPECT_CALL(*response, setParameter("path", "source_id/new_name"));
  EXPECT_CALL(*mock.http(),
              create("https://cloud-api.yandex.net/v1/disk/resources/move",
                     "POST", true))
      .WillOnce(Return(response));

  EXPECT_CALL(*mock.http(), create("status-url", "GET", true))
      .WillOnce(Return(MockResponse(R"js({ "status": "in-progress" })js")))
      .WillOnce(Return(MockResponse(R"js({ "status": "in-progress" })js")))
      .WillOnce(Return(MockResponse(R"js({ "status": "success" })js")));

  auto item =
      std::make_shared<Item>("source_filename", "source_id", IItem::UnknownSize,
                             IItem::UnknownTimeStamp, IItem::FileType::Image);

  ExpectImmediatePromise(
      provider->renameItem(item, "new_name"),
      Pointee(AllOf(Property(&IItem::filename, "new_name"),
                    Property(&IItem::id, "source_id/new_name"),
                    Property(&IItem::type, IItem::FileType::Image))));
}

TEST(YandexDiskTest, DownloadsItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("yandex", {});

  auto response = MockResponse(R"js({ "href": "file-url" })js");
  EXPECT_CALL(*response, setParameter("path", "id")).WillRepeatedly(Return());

  EXPECT_CALL(*mock.http(),
              create("https://cloud-api.yandex.net/v1/disk/resources/download",
                     "GET", true))
      .WillRepeatedly(Return(response));
  EXPECT_CALL(*mock.http(), create("file-url", "GET", true))
      .WillRepeatedly(Return(MockResponse("content")));

  auto item =
      std::make_shared<Item>("filename", "id", IItem::UnknownSize,
                             IItem::UnknownTimeStamp, IItem::FileType::Unknown);

  auto stream = std::make_shared<std::stringstream>();
  ExpectImmediatePromise(provider->downloadFile(
      item, FullRange, provider->streamDownloader(stream)));

  EXPECT_EQ(stream->str(), "content");
}

TEST(YandexDiskTest, UploadsItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("yandex", {});

  auto upload_url_request = MockResponse(R"js({ "href": "upload-url" })js");
  EXPECT_CALL(*upload_url_request, setParameter("path", "id/name"));
  EXPECT_CALL(*mock.http(),
              create("https://cloud-api.yandex.net/v1/disk/resources/upload",
                     "GET", true))
      .WillOnce(Return(upload_url_request));

  auto upload_request = MockResponse("", "content");
  EXPECT_CALL(*mock.http(), create("upload-url", "PUT", true))
      .WillOnce(Return(upload_request));

  auto parent = std::make_shared<Item>("filename", "id", IItem::UnknownSize,
                                       IItem::UnknownTimeStamp,
                                       IItem::FileType::Directory);

  auto stream = std::make_shared<std::stringstream>();
  *stream << "content";
  ExpectImmediatePromise(
      provider->uploadFile(parent, "name", provider->streamUploader(stream)),
      Pointee(AllOf(Property(&IItem::filename, "name"),
                    Property(&IItem::id, "id/name"),
                    Property(&IItem::size, 7))));
}

TEST(YandexDiskTest, ExchangesAuthorizationCode) {
  auto mock = CloudFactoryMock::create();
  ICloudFactory::ProviderInitData data;
  data.hints_["client_id"] = "client_id";
  data.hints_["client_secret"] = "client_secret";
  data.hints_["redirect_uri"] = "http://redirect-uri/";

  auto response = MockResponse(
      R"js({
             "refresh_token": "token",
             "access_token": "access_token",
             "expires_in": 0
           })js",
      R"(grant_type=authorization_code&client_id=client_id&client_secret=client_secret&code=code)");

  EXPECT_CALL(*mock.http(),
              create("https://oauth.yandex.com/token", "POST", true))
      .WillOnce(Return(MockResponse(200)))
      .WillOnce(Return(response));

  ExpectImmediatePromise(
      mock.factory()->exchangeAuthorizationCode("yandex", data, "code"),
      AllOf(Field(&Token::token_, "access_token"),
            Field(&Token::access_token_, "access_token")));
}

TEST(YandexDiskTest, GetsAuthorizeLibraryUrl) {
  auto mock = CloudFactoryMock::create();
  ICloudFactory::ProviderInitData data;
  data.hints_["client_id"] = "client_id";
  data.hints_["client_secret"] = "client_secret";
  data.hints_["redirect_uri"] = "http://redirect-uri/";
  data.hints_["state"] = "state";

  EXPECT_EQ(
      mock.factory()->authorizationUrl("yandex", data),
      R"(https://oauth.yandex.com/authorize?response_type=code&client_id=client_id&state=state&redirect_uri=http://redirect-uri/)");
}

TEST(YandexDiskTest, RootRefersToDisk) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("yandex", {});

  EXPECT_EQ(provider->root()->id(), "disk:/");
}

}  // namespace cloudstorage
