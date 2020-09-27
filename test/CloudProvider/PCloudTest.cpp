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

  ExpectHttp(mock.http(), "/userinfo")
      .WillRespondWith(
          R"js({ "email": "admin@admin.ru", "quota": 10, "usedquota": 5 })js");

  ExpectImmediatePromise(provider->generalData(),
                         AllOf(Field(&GeneralData::username_, "admin@admin.ru"),
                               Field(&GeneralData::space_total_, 10),
                               Field(&GeneralData::space_used_, 5)));
}

TEST(PCloudTest, GetsItemDataForDirectory) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("pcloud", {});

  ExpectHttp(mock.http(), "/listfolder")
      .WithParameter("nofiles", "1")
      .WithParameter("folderid", "file_id")
      .WithParameter("timeformat", "timestamp")
      .WillRespondWith(R"js({
                              "metadata": {
                                "name": "filename",
                                "folderid": "file_id",
                                "size": 1234,
                                "isfolder": true,
                                "modified": 1334203572
                              }
                            })js");

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

  ExpectHttp(mock.http(), "/checksumfile")
      .WithParameter("fileid", "file_id")
      .WithParameter("timeformat", "timestamp")
      .WillRespondWith(R"js({
                              "metadata": {
                                "name": "filename",
                                "fileid": "file_id",
                                "size": 1234,
                                "isfolder": false,
                                "modified": 1334203572
                              }
                            })js");

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

  ExpectHttp(mock.http(), "/listfolder")
      .WithParameter("folderid", "folder_id")
      .WithParameter("timeformat", "timestamp")
      .WillRespondWith(R"({
                            "metadata": {
                              "contents": [{}, {}, {}, {}, {}]
                            }
                          })");

  auto directory = std::make_shared<Item>(
      "filename", util::FileId(true, "folder_id"), IItem::UnknownSize,
      IItem::UnknownTimeStamp, IItem::FileType::Directory);

  ExpectImmediatePromise(provider->listDirectory(directory), SizeIs(5));
}

TEST(PCloudTest, DeletesItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("pcloud", {});

  ExpectHttp(mock.http(), "/deletefile")
      .WithParameter("fileid", "file_id")
      .WillRespondWithCode(200);

  auto item = std::make_shared<Item>(
      "filename", util::FileId(false, "file_id"), IItem::UnknownSize,
      IItem::UnknownTimeStamp, IItem::FileType::Unknown);

  ExpectImmediatePromise(provider->deleteItem(item));
}

TEST(PCloudTest, DeletesFolder) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("pcloud", {});

  ExpectHttp(mock.http(), "/deletefolderrecursive")
      .WithParameter("folderid", "folder_id")
      .WillRespondWithCode(200);

  auto item = std::make_shared<Item>(
      "folder", util::FileId(true, "folder_id"), IItem::UnknownSize,
      IItem::UnknownTimeStamp, IItem::FileType::Directory);

  ExpectImmediatePromise(provider->deleteItem(item));
}

TEST(PCloudTest, CreatesDirectory) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("pcloud", {});

  ExpectHttp(mock.http(), "/createfolder")
      .WithParameter("folderid", "folder_id")
      .WithParameter("name", "directory")
      .WithParameter("timeformat", "timestamp")
      .WillRespondWith(
          R"js({ "metadata": { "name": "directory", "isfolder": true } })js");

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

  ExpectHttp(mock.http(), "/renamefile")
      .WithParameter("fileid", "source_id")
      .WithParameter("tofolderid", "destination_id")
      .WithParameter("timeformat", "timestamp")
      .WillRespondWith(R"js({ "metadata": { "name": "filename" } })js");

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

  ExpectHttp(mock.http(), "/renamefolder")
      .WithParameter("folderid", "source_id")
      .WithParameter("tofolderid", "destination_id")
      .WithParameter("timeformat", "timestamp")
      .WillRespondWith(
          R"js({ "metadata": { "name": "filename", "isfolder": true } })js");

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

  ExpectHttp(mock.http(), "/renamefile")
      .WithParameter("fileid", "source_id")
      .WithParameter("toname", "new_name")
      .WithParameter("timeformat", "timestamp")
      .WillRespondWith(R"js({ "metadata": { "name": "filename" } })js");

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

  ExpectHttp(mock.http(), "/renamefolder")
      .WithParameter("folderid", "source_id")
      .WithParameter("toname", "new_name")
      .WithParameter("timeformat", "timestamp")
      .WillRespondWith(
          R"js({ "metadata": { "name": "filename", "isfolder": true } })js");

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

  ExpectHttp(mock.http(), "/getfilelink")
      .WithParameter("fileid", "id")
      .WillRespondWith(R"js({ "hosts": [ "file-url" ], "path": "/path" })js");

  ExpectHttp(mock.http(), "https://file-url/path").WillRespondWith("content");

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

  ExpectHttp(mock.http(), "/uploadfile")
      .WithMethod("POST")
      .WithHeaderParameter("Content-Type", "application/octet-stream")
      .WithHeaderParameter("Authorization", _)
      .WithParameter("folderid", "id")
      .WithParameter("filename", "name")
      .WithParameter("timeformat", "timestamp")
      .WithBody("content")
      .WillRespondWith(R"({ "metadata": [{ "name": "name" }] })");

  auto parent = std::make_shared<Item>(
      "filename", util::FileId(true, "id"), IItem::UnknownSize,
      IItem::UnknownTimeStamp, IItem::FileType::Directory);

  auto stream = std::make_shared<std::stringstream>();
  *stream << "content";
  ExpectImmediatePromise(
      provider->uploadFile(parent, "name", provider->streamUploader(stream)),
      Pointee(Property(&IItem::filename, "name")));
}

TEST(PCloudTest, ExchangesAuthorizationCode) {
  auto mock = CloudFactoryMock::create();
  ICloudFactory::ProviderInitData data;
  data.hints_["client_id"] = "client_id";
  data.hints_["client_secret"] = "client_secret";
  data.hints_["redirect_uri"] = "http://redirect-uri/";

  ExpectHttp(mock.http(), "hostname/oauth2_token")
      .WithParameter("client_id", "client_id")
      .WithParameter("client_secret", "client_secret")
      .WithParameter("code", "code")
      .WillRespondWith(
          R"js({
                 "refresh_token": "token",
                 "access_token": "access_token",
                 "expires_in": 0
               })js");

  ExpectImmediatePromise(
      mock.factory()->exchangeAuthorizationCode(
          "pcloud", data, R"({"code": "code", "hostname": "hostname"})"),
      AllOf(Field(&Token::token_,
                  IgnoringWhitespace(R"js({
                                            "hostname": "hostname",
                                            "token": "access_token"
                                          })js")),
            Field(&Token::access_token_,
                  IgnoringWhitespace(R"js({
                                            "hostname": "hostname",
                                            "token": "access_token"
                                          })js"))));
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
  ICloudFactory::ProviderInitData data;
  data.token_ = R"js({ "token": "access_token", "hostname": "hostname" })js";
  auto provider = mock.factory()->create("pcloud", data);

  ExpectHttp(mock.http(), "hostname/userinfo")
      .WillRespondWith(HttpResponse().WithHeaders({{"x-error", "1000"}}));

  ExpectFailedPromise(provider->generalData(), _);
}

TEST(PCloudTest, UsesToken) {
  auto mock = CloudFactoryMock::create();
  ICloudFactory::ProviderInitData data;
  data.token_ = R"js({ "token": "access_token", "hostname": "hostname" })js";
  auto provider = mock.factory()->create("pcloud", data);

  ExpectHttp(mock.http(), "hostname/userinfo")
      .WithHeaderParameter("Authorization", "Bearer access_token")
      .WillRespondWithCode(200);

  ExpectFailedPromise(provider->generalData(), _);
}

}  // namespace cloudstorage
