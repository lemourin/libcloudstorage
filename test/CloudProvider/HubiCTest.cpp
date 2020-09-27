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

TEST(HubiCTest, GetsGeneralData) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("hubic", {});

  ExpectHttp(mock.http(), "https://api.hubic.com/1.0/account")
      .WillRespondWith(R"js({ "email": "admin@admin.ru" })js");

  ExpectHttp(mock.http(), "https://api.hubic.com/1.0/account/usage")
      .WillRespondWith(R"js({ "quota": 10, "used": 5 })js");

  ExpectImmediatePromise(provider->generalData(),
                         AllOf(Field(&GeneralData::username_, "admin@admin.ru"),
                               Field(&GeneralData::space_total_, 10),
                               Field(&GeneralData::space_used_, 5)));
}

TEST(HubiCTest, GetsItemData) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("hubic", {});

  ExpectHttp(mock.http(), "/default")
      .WithParameter("format", "json")
      .WithParameter("prefix", "id")
      .WithParameter("delimiter", "/")
      .WithParameter("limit", "1")
      .WillRespondWith(R"([{
                             "name": "path/filename",
                             "bytes": 1234,
                             "content_type": "image/jpeg",
                             "last_modified": "2012-04-12T04:06:12.4213"
                           }])");

  ExpectImmediatePromise(
      provider->getItemData("id"),
      Pointee(AllOf(
          Property(&IItem::filename, "filename"),
          Property(&IItem::id, "path/filename"), Property(&IItem::size, 1234),
          Property(&IItem::timestamp,
                   std::chrono::system_clock::from_time_t(1334203572)))));
}

TEST(HubiCTest, ListsDirectory) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("hubic", {});

  ExpectHttp(mock.http(), "/default/")
      .WithParameter("format", "json")
      .WithParameter("marker", "")
      .WithParameter("path", "id")
      .WillRespondWith(R"([{"subdir": true}, {"name": "marker"}])")
      .AndThen()
      .WithParameter("format", "json")
      .WithParameter("marker", "marker")
      .WithParameter("path", "id")
      .WillRespondWith("[]");

  ExpectImmediatePromise(
      provider->listDirectory(std::make_unique<Item>(
          "directory", "id", IItem::UnknownSize, IItem::UnknownTimeStamp,
          IItem::FileType::Directory)),
      SizeIs(1));
}

TEST(HubiCTest, DeletesItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("hubic", {});

  ExpectHttp(mock.http(), "/default/id")
      .WithMethod("DELETE")
      .WillRespondWithCode(200);

  ExpectImmediatePromise(provider->deleteItem(std::make_unique<Item>(
      "directory", "id", IItem::UnknownSize, IItem::UnknownTimeStamp,
      IItem::FileType::Unknown)));
}

TEST(HubiCTest, CreatesDirectory) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("hubic", {});

  ExpectHttp(mock.http(), "/default/id%2Fnew_directory")
      .WithMethod("PUT")
      .WithHeaderParameter("Content-Type", "application/directory")
      .WithHeaderParameter("Authorization", _)
      .WithHeaderParameter("X-Auth-Token", _)
      .WillRespondWithCode(200);

  ExpectImmediatePromise(
      provider->createDirectory(
          std::make_unique<Item>("directory", "id", IItem::UnknownSize,
                                 IItem::UnknownTimeStamp,
                                 IItem::FileType::Directory),
          "new_directory"),
      Pointee(AllOf(Property(&IItem::filename, "new_directory"),
                    Property(&IItem::id, "id/new_directory"))));
}

TEST(HubiCTest, DownloadsItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("hubic", {});

  ExpectHttp(mock.http(), "/default/id").WillRespondWith("content");

  auto stream = std::make_shared<std::stringstream>();
  ExpectImmediatePromise(provider->downloadFile(
      std::make_unique<Item>("file", "id", IItem::UnknownSize,
                             IItem::UnknownTimeStamp, IItem::FileType::Unknown),
      FullRange, provider->streamDownloader(stream)));

  EXPECT_EQ(stream->str(), "content");
}

TEST(HubiCTest, MovesItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("hubic", {});

  ExpectHttp(mock.http(), "/default/%2Froot%2Fsource_id")
      .WithMethod("COPY")
      .WithHeaderParameter("Authorization", _)
      .WithHeaderParameter("X-Auth-Token", _)
      .WithHeaderParameter(
          "Destination",
          "/default/%2Froot%2Fsome_directory%2Fdestination%2Fsource_id")
      .WillRespondWithCode(200);

  ExpectHttp(mock.http(), "/default/%2Froot%2Fsource_id")
      .WithMethod("DELETE")
      .WillRespondWithCode(200);

  auto source =
      std::make_shared<Item>("source_id", "/root/source_id", IItem::UnknownSize,
                             IItem::UnknownTimeStamp, IItem::FileType::Unknown);
  auto destination = std::make_shared<Item>(
      "destination_id", "/root/some_directory/destination", IItem::UnknownSize,
      IItem::UnknownTimeStamp, IItem::FileType::Directory);

  ExpectImmediatePromise(
      provider->moveItem(source, destination),
      Pointee(AllOf(
          Property(&IItem::filename, "source_id"),
          Property(&IItem::id, "/root/some_directory/destination/source_id"))));
}

TEST(HubiCTest, RenamesItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("hubic", {});

  ExpectHttp(mock.http(), "/default/%2Froot%2Fsource_id")
      .WithMethod("COPY")
      .WithHeaderParameter("Authorization", _)
      .WithHeaderParameter("X-Auth-Token", _)
      .WithHeaderParameter("Destination", "/default/%2Froot%2Fnew_name")
      .WillRespondWithCode(200);

  ExpectHttp(mock.http(), "/default/%2Froot%2Fsource_id")
      .WithMethod("DELETE")
      .WillRespondWithCode(200);

  auto item =
      std::make_shared<Item>("source_id", "/root/source_id", IItem::UnknownSize,
                             IItem::UnknownTimeStamp, IItem::FileType::Unknown);

  ExpectImmediatePromise(
      provider->renameItem(item, "new_name"),
      Pointee(AllOf(Property(&IItem::filename, "new_name"),
                    Property(&IItem::id, "/root/new_name"))));
}

TEST(HubiCTest, UploadsItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("hubic", {});

  ExpectHttp(mock.http(), "/default/parent_id%2Ffilename")
      .WithMethod("PUT")
      .WithBody("content")
      .WillRespondWithCode(200);

  auto parent = std::make_shared<Item>(
      "directory", "parent_id", IItem::UnknownSize, IItem::UnknownTimeStamp,
      IItem::FileType::Directory);

  auto stream = std::make_shared<std::stringstream>();
  *stream << "content";
  ExpectImmediatePromise(provider->uploadFile(parent, "filename",
                                              provider->streamUploader(stream)),
                         Pointee(Property(&IItem::filename, "filename")));
}

TEST(HubiCTest, Authorizes) {
  auto mock = CloudFactoryMock::create();

  ICloudFactory::ProviderInitData data;
  data.hints_["client_id"] = "client_id";
  data.hints_["client_secret"] = "client_secret";

  auto provider = mock.factory()->create("hubic", data);

  ExpectHttp(mock.http(), "https://api.hubic.com/1.0/account")
      .WillRespondWithCode(401)
      .WillRespondWith("{}");

  ExpectHttp(mock.http(), "https://api.hubic.com/1.0/account/usage")
      .WillRespondWith("{}");

  ExpectHttp(mock.http(), "https://api.hubic.com/oauth/token")
      .WithMethod("POST")
      .WithBody(
          R"(grant_type=refresh_token&refresh_token=&client_id=client_id&client_secret=client_secret)")
      .WillRespondWith(R"js({ "access_token": "access_token" })js");

  ExpectHttp(mock.http(), "https://api.hubic.com/1.0/account/credentials")
      .WillRespondWith(R"js({
                              "endpoint": "openstack-endpoint",
                              "token": "openstack-token"
                            })js");

  ExpectHttp(mock.http(), "openstack-endpoint/default")
      .WithMethod("POST")
      .WithHeaderParameter("X-Container-Meta-Temp-URL-Key", "openstack-token")
      .WithHeaderParameter("X-Auth-Token", "openstack-token")
      .WithHeaderParameter("Authorization", "Bearer access_token")
      .WillRespondWithCode(200);

  ExpectImmediatePromise(provider->generalData(), _);
}

TEST(HubiCTest, ExchangesAuthorizationCode) {
  auto mock = CloudFactoryMock::create();
  ICloudFactory::ProviderInitData data;
  data.hints_["client_id"] = "client_id";
  data.hints_["client_secret"] = "client_secret";
  data.hints_["redirect_uri"] = "http://redirect-uri/";

  ExpectHttp(mock.http(), "https://api.hubic.com/oauth/token")
      .WithMethod("POST")
      .WithBody(
          R"(grant_type=authorization_code&redirect_uri=http%3A%2F%2Fredirect-uri%2F&code=code&client_id=client_id&client_secret=client_secret)")
      .WillRespondWith(R"js({
                              "refresh_token": "token",
                              "access_token": "access_token",
                              "expires_in": 0
                            })js");

  ExpectImmediatePromise(
      mock.factory()->exchangeAuthorizationCode("hubic", data, "code"),
      AllOf(Field(&Token::token_, "token"),
            Field(&Token::access_token_, "access_token")));
}

TEST(HubiCTest, GetsAuthorizeLibraryUrl) {
  auto mock = CloudFactoryMock::create();
  ICloudFactory::ProviderInitData data;
  data.hints_["client_id"] = "client_id";
  data.hints_["client_secret"] = "client_secret";
  data.hints_["redirect_uri"] = "http://redirect-uri/";
  data.hints_["state"] = "state";

  EXPECT_EQ(
      mock.factory()->authorizationUrl("hubic", data),
      R"(https://api.hubic.com/oauth/auth?client_id=client_id&response_type=code&redirect_uri=http://redirect-uri/&state=state&scope=credentials.r%2Caccount.r%2Cusage.r)");
}

}  // namespace cloudstorage
