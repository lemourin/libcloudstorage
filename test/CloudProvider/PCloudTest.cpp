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

TEST(PCloudTest, GetsGeneralData) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("pcloud", {});

  EXPECT_CALL(*mock.http(),
              create("https://api.pcloud.com/userinfo", "GET", true))
      .WillOnce(Return(MockResponse(R"js({
                                           "email": "admin@admin.ru",
                                           "quota": 10,
                                           "usedquota": 5
                                         })js")));

  ExpectImmediatePromise(provider->generalData(),
                         AllOf(Field(&GeneralData::username_, "admin@admin.ru"),
                               Field(&GeneralData::space_total_, 10),
                               Field(&GeneralData::space_used_, 5)));
}

TEST(PCloudTest, GetsItemDataForDirectory) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("pcloud", {});

  auto response = MockResponse(R"js({
                                      "metadata": {
                                        "name": "filename",
                                        "folderid": "file_id",
                                        "size": 1234,
                                        "isfolder": true,
                                        "modified": 1334203572
                                      }
                                    })js");
  EXPECT_CALL(*response, setParameter("nofiles", "1"));
  EXPECT_CALL(*response, setParameter("folderid", "file_id"));
  EXPECT_CALL(*response, setParameter("timeformat", "timestamp"));

  EXPECT_CALL(*mock.http(),
              create("https://api.pcloud.com/listfolder", "GET", true))
      .WillOnce(Return(response));

  ExpectImmediatePromise(
      provider->getItemData(util::FileId(true, "file_id")),
      Pointee(
          AllOf(Property(&IItem::filename, "filename"),
                Property(&IItem::id, util::FileId(true, "file_id")),
                Property(&IItem::size, 1234),
                Property(&IItem::type, IItem::FileType::Directory),
                Property(&IItem::timestamp,
                         std::chrono::system_clock::from_time_t(1334203572)))));
}

TEST(PCloudTest, GetsItemDataForItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("pcloud", {});

  auto response = MockResponse(R"js({
                                      "metadata": {
                                        "name": "filename",
                                        "fileid": "file_id",
                                        "size": 1234,
                                        "isfolder": false,
                                        "modified": 1334203572
                                      }
                                    })js");
  EXPECT_CALL(*response, setParameter("fileid", "file_id"));
  EXPECT_CALL(*response, setParameter("timeformat", "timestamp"));

  EXPECT_CALL(*mock.http(),
              create("https://api.pcloud.com/checksumfile", "GET", true))
      .WillOnce(Return(response));

  ExpectImmediatePromise(
      provider->getItemData(util::FileId(false, "file_id")),
      Pointee(
          AllOf(Property(&IItem::filename, "filename"),
                Property(&IItem::id, util::FileId(false, "file_id")),
                Property(&IItem::size, 1234),
                Property(&IItem::type, IItem::FileType::Unknown),
                Property(&IItem::timestamp,
                         std::chrono::system_clock::from_time_t(1334203572)))));
}

TEST(PCloudTest, ListsDirectory) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("pcloud", {});

  auto response = MockResponse(
      R"({
           "metadata": {
             "contents": [{}, {}, {}, {}, {}]
           }
         })");
  EXPECT_CALL(*response, setParameter("folderid", "folder_id"));
  EXPECT_CALL(*response, setParameter("timeformat", "timestamp"));

  EXPECT_CALL(*mock.http(),
              create("https://api.pcloud.com/listfolder", "GET", true))
      .WillOnce(Return(response));

  auto directory = std::make_shared<Item>(
      "filename", util::FileId(true, "folder_id"), IItem::UnknownSize,
      IItem::UnknownTimeStamp, IItem::FileType::Directory);

  ExpectImmediatePromise(provider->listDirectory(directory), SizeIs(5));
}

TEST(PCloudTest, DeletesItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("pcloud", {});

  auto response = MockResponse(200);
  EXPECT_CALL(*response, setParameter("fileid", "file_id"));

  EXPECT_CALL(*mock.http(),
              create("https://api.pcloud.com/deletefile", "GET", true))
      .WillOnce(Return(response));

  auto item = std::make_shared<Item>(
      "filename", util::FileId(false, "file_id"), IItem::UnknownSize,
      IItem::UnknownTimeStamp, IItem::FileType::Unknown);

  ExpectImmediatePromise(provider->deleteItem(item));
}

TEST(PCloudTest, DeletesFolder) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("pcloud", {});

  auto response = MockResponse(200);
  EXPECT_CALL(*response, setParameter("folderid", "folder_id"));

  EXPECT_CALL(
      *mock.http(),
      create("https://api.pcloud.com/deletefolderrecursive", "GET", true))
      .WillOnce(Return(response));

  auto item = std::make_shared<Item>(
      "folder", util::FileId(true, "folder_id"), IItem::UnknownSize,
      IItem::UnknownTimeStamp, IItem::FileType::Directory);

  ExpectImmediatePromise(provider->deleteItem(item));
}

TEST(PCloudTest, CreatesDirectory) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("pcloud", {});

  auto response = MockResponse(
      R"js({ "metadata": { "name": "directory", "isfolder": true } })js");
  EXPECT_CALL(*response, setParameter("folderid", "folder_id"));
  EXPECT_CALL(*response, setParameter("name", "directory"));
  EXPECT_CALL(*response, setParameter("timeformat", "timestamp"));
  EXPECT_CALL(*mock.http(),
              create("https://api.pcloud.com/createfolder", "GET", true))
      .WillOnce(Return(response));

  auto parent = std::make_shared<Item>(
      "parent", util::FileId(true, "folder_id"), IItem::UnknownSize,
      IItem::UnknownTimeStamp, IItem::FileType::Directory);

  ExpectImmediatePromise(
      provider->createDirectory(parent, "directory"),
      Pointee(AllOf(Property(&IItem::filename, "directory"),
                    Property(&IItem::type, IItem::FileType::Directory))));
}

TEST(PCloudTest, MovesItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("pcloud", {});

  auto response = MockResponse(R"js({ "metadata": { "name": "filename" } })js");
  EXPECT_CALL(*response, setParameter("fileid", "source_id"));
  EXPECT_CALL(*response, setParameter("tofolderid", "destination_id"));
  EXPECT_CALL(*response, setParameter("timeformat", "timestamp"));
  EXPECT_CALL(*mock.http(),
              create("https://api.pcloud.com/renamefile", "GET", true))
      .WillOnce(Return(response));

  auto source = std::make_shared<Item>(
      "source", util::FileId(false, "source_id"), IItem::UnknownSize,
      IItem::UnknownTimeStamp, IItem::FileType::Unknown);

  auto destination = std::make_shared<Item>(
      "destination", util::FileId(true, "destination_id"), IItem::UnknownSize,
      IItem::UnknownTimeStamp, IItem::FileType::Directory);

  ExpectImmediatePromise(
      provider->moveItem(source, destination),
      Pointee(AllOf(Property(&IItem::filename, "filename"),
                    Property(&IItem::type, IItem::FileType::Unknown))));
}

TEST(PCloudTest, MovesFolder) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("pcloud", {});

  auto response = MockResponse(
      R"js({ "metadata": { "name": "filename", "isfolder": true } })js");
  EXPECT_CALL(*response, setParameter("folderid", "source_id"));
  EXPECT_CALL(*response, setParameter("tofolderid", "destination_id"));
  EXPECT_CALL(*response, setParameter("timeformat", "timestamp"));
  EXPECT_CALL(*mock.http(),
              create("https://api.pcloud.com/renamefolder", "GET", true))
      .WillOnce(Return(response));

  auto source = std::make_shared<Item>(
      "source", util::FileId(true, "source_id"), IItem::UnknownSize,
      IItem::UnknownTimeStamp, IItem::FileType::Directory);

  auto destination = std::make_shared<Item>(
      "destination", util::FileId(true, "destination_id"), IItem::UnknownSize,
      IItem::UnknownTimeStamp, IItem::FileType::Directory);

  ExpectImmediatePromise(
      provider->moveItem(source, destination),
      Pointee(AllOf(Property(&IItem::filename, "filename"),
                    Property(&IItem::type, IItem::FileType::Directory))));
}

TEST(PCloudTest, RenamesItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("pcloud", {});

  auto response = MockResponse(R"js({ "metadata": { "name": "filename" } })js");
  EXPECT_CALL(*response, setParameter("fileid", "source_id"));
  EXPECT_CALL(*response, setParameter("toname", "new_name"));
  EXPECT_CALL(*response, setParameter("timeformat", "timestamp"));
  EXPECT_CALL(*mock.http(),
              create("https://api.pcloud.com/renamefile", "GET", true))
      .WillOnce(Return(response));

  auto source = std::make_shared<Item>(
      "source", util::FileId(false, "source_id"), IItem::UnknownSize,
      IItem::UnknownTimeStamp, IItem::FileType::Unknown);

  ExpectImmediatePromise(
      provider->renameItem(source, "new_name"),
      Pointee(AllOf(Property(&IItem::filename, "filename"),
                    Property(&IItem::type, IItem::FileType::Unknown))));
}

TEST(PCloudTest, RenamesFolder) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("pcloud", {});

  auto response = MockResponse(
      R"js({ "metadata": { "name": "filename", "isfolder": true } })js");
  EXPECT_CALL(*response, setParameter("folderid", "source_id"));
  EXPECT_CALL(*response, setParameter("toname", "new_name"));
  EXPECT_CALL(*response, setParameter("timeformat", "timestamp"));
  EXPECT_CALL(*mock.http(),
              create("https://api.pcloud.com/renamefolder", "GET", true))
      .WillOnce(Return(response));

  auto source = std::make_shared<Item>(
      "source", util::FileId(true, "source_id"), IItem::UnknownSize,
      IItem::UnknownTimeStamp, IItem::FileType::Directory);

  ExpectImmediatePromise(
      provider->renameItem(source, "new_name"),
      Pointee(AllOf(Property(&IItem::filename, "filename"),
                    Property(&IItem::type, IItem::FileType::Directory))));
}

TEST(PCloudTest, DownloadsItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("pcloud", {});

  auto response =
      MockResponse(R"js({ "hosts": [ "file-url" ], "path": "/path" })js");
  EXPECT_CALL(*response, setParameter("fileid", "id")).WillRepeatedly(Return());

  EXPECT_CALL(*mock.http(),
              create("https://api.pcloud.com/getfilelink", "GET", true))
      .WillRepeatedly(Return(response));
  EXPECT_CALL(*mock.http(), create("https://file-url/path", "GET", true))
      .WillRepeatedly(Return(MockResponse("content")));

  auto item = std::make_shared<Item>(
      "filename", util::FileId(false, "id"), IItem::UnknownSize,
      IItem::UnknownTimeStamp, IItem::FileType::Unknown);

  auto stream = std::make_shared<std::stringstream>();
  ExpectImmediatePromise(provider->downloadFile(
      item, FullRange, provider->streamDownloader(stream)));

  EXPECT_EQ(stream->str(), "content");
}

TEST(PCloudTest, UploadsItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("pcloud", {});

  auto response =
      MockResponse(R"({ "metadata": [{ "name": "name" }] })", "content");
  EXPECT_CALL(*response,
              setHeaderParameter("Content-Type", "application/octet-stream"));
  EXPECT_CALL(*response, setHeaderParameter("Authorization", _));
  EXPECT_CALL(*response, setParameter("folderid", "id"));
  EXPECT_CALL(*response, setParameter("filename", "name"));
  EXPECT_CALL(*response, setParameter("timeformat", "timestamp"));

  EXPECT_CALL(*mock.http(),
              create("https://api.pcloud.com/uploadfile", "POST", true))
      .WillOnce(Return(response));

  auto parent = std::make_shared<Item>(
      "filename", util::FileId(true, "id"), IItem::UnknownSize,
      IItem::UnknownTimeStamp, IItem::FileType::Directory);

  auto stream = std::make_shared<std::stringstream>();
  *stream << "content";
  ExpectImmediatePromise(
      provider->uploadFile(parent, "name", provider->streamUploader(stream)),
      Pointee(Property(&IItem::filename, "name")));
}

TEST(PCloudTest, RefreshesToken) {
  auto mock = CloudFactoryMock::create();
  ICloudFactory::ProviderInitData data;
  data.hints_["client_id"] = "client_id";
  data.hints_["client_secret"] = "client_secret";
  data.hints_["redirect_uri"] = "http://redirect-uri/";
  data.token_ = "refresh_token";
  auto provider = mock.factory()->create("box", data);

  auto successful_response = MockResponse(R"js({})js");
  EXPECT_CALL(*successful_response,
              setHeaderParameter("Authorization", "Bearer token"));

  EXPECT_CALL(*mock.http(),
              create("https://api.box.com/2.0/users/me", "GET", true))
      .WillOnce(Return(MockResponse(401, "")))
      .WillOnce(Return(successful_response));

  EXPECT_CALL(*mock.http(),
              create("https://api.box.com/oauth2/token", "POST", true))
      .WillOnce(Return(MockResponse(
          R"js({ "access_token": "token" })js",
          R"(grant_type=refresh_token&refresh_token=refresh_token&client_id=client_id&client_secret=client_secret&redirect_uri=http://redirect-uri/)")));

  ExpectImmediatePromise(provider->generalData(), _);
}

TEST(PCloudTest, ExchangesAuthorizationCode) {
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
           })js");
  EXPECT_CALL(*response, setParameter("client_id", "client_id"));
  EXPECT_CALL(*response, setParameter("client_secret", "client_secret"));
  EXPECT_CALL(*response, setParameter("code", "code"));

  EXPECT_CALL(*mock.http(),
              create("https://api.pcloud.com/oauth2_token", "GET", true))
      .WillOnce(Return(MockResponse(200)))
      .WillOnce(Return(response));

  ExpectImmediatePromise(
      mock.factory()->exchangeAuthorizationCode("pcloud", data, "code"),
      AllOf(Field(&Token::token_, "access_token"),
            Field(&Token::access_token_, "access_token")));
}

TEST(PCloudTest, GetsAuthorizeLibraryUrl) {
  auto mock = CloudFactoryMock::create();
  ICloudFactory::ProviderInitData data;
  data.hints_["client_id"] = "client_id";
  data.hints_["client_secret"] = "client_secret";
  data.hints_["redirect_uri"] = "http://redirect-uri/";
  data.hints_["state"] = "state";

  EXPECT_EQ(
      mock.factory()->authorizationUrl("pcloud", data),
      R"(https://my.pcloud.com/oauth2/authorize?client_id=client_id&response_type=code&redirect_uri=http%3A%2F%2Fredirect-uri%2F&state=state)");
}

TEST(PCloudTest, RootUsesFileId) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("pcloud", {});

  EXPECT_EQ(provider->root()->id(), std::string(util::FileId(true, "0")));
}

TEST(PCloudTest, HandlesFailure) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("pcloud", {});

  EXPECT_CALL(*mock.http(),
              create("https://api.pcloud.com/userinfo", "GET", true))
      .WillOnce(Return(
          MockResponse(200, IHttpRequest::HeaderParameters{{"x-error", "1000"}},
                       "unauthorized", _)));

  ExpectFailedPromise(provider->generalData(), _);
}

}  // namespace cloudstorage
