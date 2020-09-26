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

  EXPECT_CALL(*mock.http(),
              create("https://api.hubic.com/1.0/account", "GET", true))
      .WillOnce(Return(MockResponse(R"js({ "email": "admin@admin.ru" })js")));
  EXPECT_CALL(*mock.http(),
              create("https://api.hubic.com/1.0/account/usage", "GET", true))
      .WillOnce(Return(MockResponse(R"js({ "quota": 10, "used": 5 })js")));

  ExpectImmediatePromise(provider->generalData(),
                         AllOf(Field(&GeneralData::username_, "admin@admin.ru"),
                               Field(&GeneralData::space_total_, 10),
                               Field(&GeneralData::space_used_, 5)));
}

TEST(HubiCTest, GetsItemData) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("hubic", {});

  auto response = MockResponse(R"([{
                                     "name": "path/filename",
                                     "bytes": 1234,
                                     "content_type": "image/jpeg",
                                     "last_modified": "2012-04-12T04:06:12.4213"
                                  }])");
  EXPECT_CALL(*response, setParameter("format", "json"));
  EXPECT_CALL(*response, setParameter("prefix", "id"));
  EXPECT_CALL(*response, setParameter("delimiter", "/"));
  EXPECT_CALL(*response, setParameter("limit", "1"));

  EXPECT_CALL(*mock.http(), create("/default", "GET", true))
      .WillOnce(Return(response));

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

  auto response_first =
      MockResponse(R"([{"subdir": true}, {"name": "marker"}])");
  EXPECT_CALL(*response_first, setParameter("format", "json"));
  EXPECT_CALL(*response_first, setParameter("marker", ""));
  EXPECT_CALL(*response_first, setParameter("path", "id"));

  auto response_second = MockResponse(R"([])");
  EXPECT_CALL(*response_second, setParameter("format", "json"));
  EXPECT_CALL(*response_second, setParameter("marker", "marker"));
  EXPECT_CALL(*response_second, setParameter("path", "id"));

  EXPECT_CALL(*mock.http(), create("/default/", "GET", true))
      .WillOnce(Return(response_first))
      .WillOnce(Return(response_second));

  ExpectImmediatePromise(
      provider->listDirectory(std::make_unique<Item>(
          "directory", "id", IItem::UnknownSize, IItem::UnknownTimeStamp,
          IItem::FileType::Directory)),
      SizeIs(1));
}

TEST(HubiCTest, DeletesItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("hubic", {});

  EXPECT_CALL(*mock.http(), create("/default/id", "DELETE", true))
      .WillOnce(Return(MockResponse("")));

  ExpectImmediatePromise(provider->deleteItem(std::make_unique<Item>(
      "directory", "id", IItem::UnknownSize, IItem::UnknownTimeStamp,
      IItem::FileType::Unknown)));
}

TEST(HubiCTest, CreatesDirectory) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("hubic", {});

  auto response = MockResponse(200);
  EXPECT_CALL(*response,
              setHeaderParameter("Content-Type", "application/directory"));
  EXPECT_CALL(*response, setHeaderParameter("Authorization", _));
  EXPECT_CALL(*response, setHeaderParameter("X-Auth-Token", _));

  EXPECT_CALL(*mock.http(), create("/default/id%2Fnew_directory", "PUT", true))
      .WillOnce(Return(response));

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

  EXPECT_CALL(*mock.http(), create("/default/id", "GET", true))
      .WillOnce(Return(MockResponse("content")));

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

  auto response = MockResponse("");
  EXPECT_CALL(*response, setHeaderParameter("Authorization", _));
  EXPECT_CALL(*response, setHeaderParameter("X-Auth-Token", _));
  EXPECT_CALL(
      *response,
      setHeaderParameter(
          "Destination",
          "/default/%2Froot%2Fsome_directory%2Fdestination%2Fsource_id"));

  EXPECT_CALL(*mock.http(),
              create("/default/%2Froot%2Fsource_id", "COPY", true))
      .WillOnce(Return(response));

  EXPECT_CALL(*mock.http(),
              create("/default/%2Froot%2Fsource_id", "DELETE", true))
      .WillOnce(Return(MockResponse("")));

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

  auto response = MockResponse("");
  EXPECT_CALL(*response, setHeaderParameter("Authorization", _));
  EXPECT_CALL(*response, setHeaderParameter("X-Auth-Token", _));
  EXPECT_CALL(*response,
              setHeaderParameter("Destination", "/default/%2Froot%2Fnew_name"));

  EXPECT_CALL(*mock.http(),
              create("/default/%2Froot%2Fsource_id", "COPY", true))
      .WillOnce(Return(response));

  EXPECT_CALL(*mock.http(),
              create("/default/%2Froot%2Fsource_id", "DELETE", true))
      .WillOnce(Return(MockResponse("")));

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

  EXPECT_CALL(*mock.http(),
              create("/default/parent_id%2Ffilename", "PUT", true))
      .WillOnce(Return(MockResponse("", "content")));

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

  EXPECT_CALL(*mock.http(),
              create("https://api.hubic.com/1.0/account", "GET", true))
      .WillOnce(Return(MockResponse(401)))
      .WillOnce(Return(MockResponse("{}")));
  EXPECT_CALL(*mock.http(),
              create("https://api.hubic.com/1.0/account/usage", "GET", true))
      .WillOnce(Return(MockResponse("{}")));

  EXPECT_CALL(*mock.http(),
              create("https://api.hubic.com/oauth/token", "POST", true))
      .WillOnce(Return(MockResponse(
          R"js({ "access_token": "access_token" })js",
          R"(grant_type=refresh_token&refresh_token=&client_id=client_id&client_secret=client_secret)")));

  EXPECT_CALL(
      *mock.http(),
      create("https://api.hubic.com/1.0/account/credentials", "GET", true))
      .WillOnce(Return(MockResponse(R"js({
                                           "endpoint": "openstack-endpoint",
                                           "token": "openstack-token"
                                         })js")));

  auto response = MockResponse("");
  EXPECT_CALL(*response, setHeaderParameter("X-Container-Meta-Temp-URL-Key",
                                            "openstack-token"));
  EXPECT_CALL(*response,
              setHeaderParameter("Authorization", "Bearer access_token"));
  EXPECT_CALL(*response, setHeaderParameter("X-Auth-Token", "openstack-token"));
  EXPECT_CALL(*mock.http(), create("openstack-endpoint/default", "POST", true))
      .WillOnce(Return(response));

  ExpectImmediatePromise(provider->generalData(), _);
}

TEST(HubiCTest, ExchangesAuthorizationCode) {
  auto mock = CloudFactoryMock::create();
  ICloudFactory::ProviderInitData data;
  data.hints_["client_id"] = "client_id";
  data.hints_["client_secret"] = "client_secret";
  data.hints_["redirect_uri"] = "http://redirect-uri/";

  EXPECT_CALL(*mock.http(),
              create("https://api.hubic.com/oauth/token", "POST", true))
      .WillOnce(Return(MockResponse(
          R"js({
                 "refresh_token": "token",
                 "access_token": "access_token",
                 "expires_in": 0
               })js",
          R"(grant_type=authorization_code&redirect_uri=http%3A%2F%2Fredirect-uri%2F&code=code&client_id=client_id&client_secret=client_secret)")));

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
