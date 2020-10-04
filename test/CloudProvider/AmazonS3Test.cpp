#include "gtest/gtest.h"

#include "Utility/CloudFactoryMock.h"
#include "Utility/Item.h"
#include "Utility/Utility.h"

namespace cloudstorage {

using testing::_;
using testing::AllOf;
using testing::Contains;
using testing::ElementsAre;
using testing::Field;
using testing::Invoke;
using testing::InvokeArgument;
using testing::Pointee;
using testing::Property;
using testing::Return;
using testing::WithArgs;

namespace {
ICloudFactory::ProviderInitData GetDefaultInitData() {
  ICloudFactory::ProviderInitData data;
  data.hints_["region"] = "region";
  data.token_ = util::encode_token(R"js({
                                          "bucket": "bucket",
                                          "endpoint": "endpoint",
                                          "username": "lemourindrive",
                                          "password": "password"
                                        })js");
  return data;
}
}  // namespace

TEST(AmazonS3Test, ListsDirectory) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("amazons3", GetDefaultInitData());

  ExpectHttp(mock.http(), _).WillRespondWithCode(200).WillRespondWithCode(200);

  ExpectHttp(mock.http(), "endpoint/bucket/")
      .WithRequestMatching(
          Property(&IHttpRequest::parameters,
                   AllOf(Contains(std::make_pair("list-type", "2")),
                         Contains(std::make_pair("delimiter", "/")),
                         Contains(std::make_pair("prefix", "video/")))))
      .WillRespondWith(R"(
        <?xml version="1.0" encoding="UTF-8"?>
        <ListBucketResult xmlns="http://s3.amazonaws.com/doc/2006-03-01/">
            <Name>lemourindrive</Name>
            <Prefix>video/</Prefix>
            <KeyCount>3</KeyCount>
            <MaxKeys>1000</MaxKeys>
            <Delimiter>/</Delimiter>
            <IsTruncated>false</IsTruncated>
            <Contents>
                <Key>video/</Key>
                <LastModified>2017-09-13T13:26:56.000Z</LastModified>
                <ETag>"d41d8cd98f00b204e9800998ecf8427e"</ETag>
                <Size>0</Size>
                <StorageClass>STANDARD</StorageClass>
            </Contents>
            <Contents>
                <Key>video/[MV] れをる - 極彩色 x Gokusaishiki.mp4</Key>
                <LastModified>2017-10-05T11:02:36.000Z</LastModified>
                <ETag>"bb7c22fef3260cb0b9cc04259c4355a2"</ETag>
                <Size>39838997</Size>
                <StorageClass>STANDARD</StorageClass>
            </Contents>
            <Contents>
                <Key>video/config.log</Key>
                <LastModified>2017-10-22T14:37:07.000Z</LastModified>
                <ETag>"181b92f7da001722a6dc686deb40447b"</ETag>
                <Size>62230</Size>
                <StorageClass>STANDARD</StorageClass>
            </Contents>
        </ListBucketResult>
      )");

  auto item = std::make_shared<Item>("video/");
  item->set_type(IItem::FileType::Directory);

  ExpectImmediatePromise(
      provider->listDirectory(item),
      ElementsAre(
          Pointee(AllOf(
              Property(&IItem::id,
                       "video/[MV] れをる - 極彩色 x Gokusaishiki.mp4"),
              Property(&IItem::filename,
                       "[MV] れをる - 極彩色 x Gokusaishiki.mp4"),
              Property(&IItem::size, 39838997),
              Property(&IItem::type, IItem::FileType::Video),
              Property(&IItem::timestamp,
                       std::chrono::system_clock::from_time_t(1507201356)))),
          Pointee(AllOf(
              Property(&IItem::id, "video/config.log"),
              Property(&IItem::filename, "config.log"),
              Property(&IItem::size, 62230),
              Property(&IItem::type, IItem::FileType::Unknown),
              Property(&IItem::timestamp,
                       std::chrono::system_clock::from_time_t(1508683027))))));
}

TEST(AmazonS3Test, GetsGeneralData) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("amazons3", GetDefaultInitData());

  ExpectImmediatePromise(
      provider->generalData(),
      AllOf(Field(&GeneralData::space_used_, 0),
            Field(&GeneralData::space_total_, 0),
            Field(&GeneralData::username_, "endpoint/bucket")));
}

TEST(AmazonS3Test, GetsItem) {
  auto mock = CloudFactoryMock::create();
  ICloudFactory::ProviderInitData data;
  data.hints_["region"] = "region";
  data.token_ = util::encode_token(R"js({
                                          "bucket": "bucket",
                                          "endpoint": "endpoint",
                                          "username": "lemourindrive",
                                          "password": "password"
                                        })js");
  auto provider = mock.factory()->create("amazons3", data);

  ExpectHttp(mock.http(), _).WillRespondWithCode(200);

  ExpectHttp(mock.http(), "endpoint/bucket/")
      .WithRequestMatching(Property(
          &IHttpRequest::parameters,
          AllOf(Contains(std::make_pair("list-type", "2")),
                Contains(std::make_pair("delimiter", "/")),
                Contains(std::make_pair("prefix", "video/config.log")))))
      .WillRespondWith(R"(
        <?xml version="1.0" encoding="UTF-8"?>
        <ListBucketResult xmlns="http://s3.amazonaws.com/doc/2006-03-01/">
            <Name>lemourindrive</Name>
            <Prefix>video/config.log</Prefix>
            <KeyCount>1</KeyCount>
            <MaxKeys>1000</MaxKeys>
            <Delimiter>/</Delimiter>
            <IsTruncated>false</IsTruncated>
            <Contents>
                <Key>video/config.log</Key>
                <LastModified>2017-10-22T14:37:07.000Z</LastModified>
                <ETag>"181b92f7da001722a6dc686deb40447b"</ETag>
                <Size>62230</Size>
                <StorageClass>STANDARD</StorageClass>
            </Contents>
        </ListBucketResult>
      )");

  ExpectImmediatePromise(
      provider->getItemData("video/config.log"),
      Pointee(
          AllOf(Property(&IItem::id, "video/config.log"),
                Property(&IItem::filename, "config.log"),
                Property(&IItem::size, 62230),
                Property(&IItem::type, IItem::FileType::Unknown),
                Property(&IItem::timestamp,
                         std::chrono::system_clock::from_time_t(1508683027)))));
}

TEST(AmazonS3Test, DeletesItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("amazons3", GetDefaultInitData());

  ExpectHttp(mock.http(), "endpoint/bucket/id")
      .WithMethod("DELETE")
      .WillRespondWithCode(200);

  ExpectImmediatePromise(provider->deleteItem(std::make_unique<Item>("id")));
}

TEST(AmazonS3Test, CreatesDirectory) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("amazons3", GetDefaultInitData());

  ExpectHttp(mock.http(), "endpoint/bucket/id/new_directory/")
      .WithMethod("PUT")
      .WillRespondWithCode(200);

  auto item = std::make_shared<Item>("id/");
  item->set_type(IItem::FileType::Directory);

  ExpectImmediatePromise(
      provider->createDirectory(item, "new_directory"),
      Pointee(AllOf(Property(&IItem::filename, "new_directory"),
                    Property(&IItem::id, "id/new_directory/"))));
}

TEST(AmazonS3Test, MovesItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("amazons3", GetDefaultInitData());

  ExpectHttp(mock.http(),
             "endpoint/bucket/root/some_directory/destination/source_id")
      .WithMethod("PUT")
      .WithRequestMatching(
          Property(&IHttpRequest::headerParameters,
                   Contains(std::make_pair("x-amz-copy-source",
                                           "bucket/root/source_id"))))
      .WillRespondWithCode(200);

  ExpectHttp(mock.http(), "endpoint/bucket/root/source_id")
      .WithMethod("DELETE")
      .WillRespondWithCode(200);

  auto source =
      std::make_shared<Item>("source_id", "root/source_id", IItem::UnknownSize,
                             IItem::UnknownTimeStamp, IItem::FileType::Unknown);
  auto destination = std::make_shared<Item>(
      "destination_id", "root/some_directory/destination/", IItem::UnknownSize,
      IItem::UnknownTimeStamp, IItem::FileType::Directory);

  ExpectImmediatePromise(
      provider->moveItem(source, destination),
      Pointee(AllOf(
          Property(&IItem::filename, "source_id"),
          Property(&IItem::id, "root/some_directory/destination/source_id"))));
}

TEST(AmazonS3Test, RenamesItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("amazons3", GetDefaultInitData());

  ExpectHttp(mock.http(), "endpoint/bucket/root/new_name")
      .WithMethod("PUT")
      .WithRequestMatching(
          Property(&IHttpRequest::headerParameters,
                   Contains(std::make_pair("x-amz-copy-source",
                                           "bucket/root/source_id"))))
      .WillRespondWithCode(200);

  ExpectHttp(mock.http(), "endpoint/bucket/root/source_id")
      .WithMethod("DELETE")
      .WillRespondWithCode(200);

  auto source =
      std::make_shared<Item>("source_id", "root/source_id", IItem::UnknownSize,
                             IItem::UnknownTimeStamp, IItem::FileType::Unknown);

  ExpectImmediatePromise(provider->renameItem(source, "new_name"),
                         Pointee(AllOf(Property(&IItem::filename, "new_name"),
                                       Property(&IItem::id, "root/new_name"))));
}

TEST(AmazonS3Test, UploadsItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("amazons3", GetDefaultInitData());

  ExpectHttp(mock.http(), "endpoint/bucket/parent_id/filename")
      .WithMethod("PUT")
      .WithBody("content")
      .WillRespondWithCode(200);

  auto parent = std::make_shared<Item>(
      "directory", "parent_id/", IItem::UnknownSize, IItem::UnknownTimeStamp,
      IItem::FileType::Directory);

  auto stream = std::make_shared<std::stringstream>();
  *stream << "content";
  ExpectImmediatePromise(provider->uploadFile(parent, "filename",
                                              provider->streamUploader(stream)),
                         Pointee(Property(&IItem::filename, "filename")));
}

TEST(AmazonS3Test, DownloadsItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("amazons3", GetDefaultInitData());

  ExpectHttp(mock.http(), "endpoint/bucket/id").WillRespondWith("content");

  auto stream = std::make_shared<std::stringstream>();
  ExpectImmediatePromise(
      provider->downloadFile(std::make_unique<Item>("id"), FullRange,
                             provider->streamDownloader(stream)));

  EXPECT_EQ(stream->str(), "content");
}

TEST(AmazonS3Test, FiguresOutRegion) {
  auto mock = CloudFactoryMock::create();
  ICloudFactory::ProviderInitData data;
  data.token_ = util::encode_token(R"js({
                                          "bucket": "bucket",
                                          "endpoint": "endpoint",
                                          "username": "lemourindrive",
                                          "password": "password"
                                        })js");
  auto provider = mock.factory()->create("amazons3", GetDefaultInitData());

  ExpectHttp(mock.http(), "endpoint/bucket/id/new_directory/")
      .WithMethod("PUT")
      .WillRespondWithCode(401)
      .WillRespondWithCode(200);

  ExpectHttp(mock.http(), "endpoint/bucket/")
      .WithRequestMatching(Property(&IHttpRequest::parameters,
                                    Contains(std::make_pair("location", ""))))
      .WillRespondWith(R"(
        <?xml version="1.0" encoding="UTF-8"?>
        <Location>location</Location>
      )")
      .AndThen()
      .WillRespondWithCode(200);

  auto item = std::make_shared<Item>("id/");
  item->set_type(IItem::FileType::Directory);

  ExpectImmediatePromise(provider->createDirectory(item, "new_directory"), _);

  EXPECT_EQ(provider->hints()["region"], "location");
}

TEST(AmazonS3Test, FiguresOutEndpointWhenEndpointRequestFails) {
  auto mock = CloudFactoryMock::create();
  ICloudFactory::ProviderInitData data;
  data.token_ = util::encode_token(R"js({
                                          "bucket": "bucket",
                                          "endpoint": "endpoint",
                                          "username": "lemourindrive",
                                          "password": "password"
                                        })js");
  auto provider = mock.factory()->create("amazons3", GetDefaultInitData());

  ExpectHttp(mock.http(), "endpoint/bucket/id/new_directory/")
      .WithMethod("PUT")
      .WillRespondWithCode(401);

  ExpectHttp(mock.http(), "new-endpoint/id/new_directory/")
      .WithMethod("PUT")
      .WillRespondWithCode(200);

  ExpectHttp(mock.http(), "endpoint/bucket/")
      .WillRespondWith(R"(
        <?xml version="1.0" encoding="UTF-8"?>
        <Location>location</Location>
      )")
      .AndThen()
      .WillRespondWith(HttpResponse().WithStatus(301).WithContent(R"(
        <?xml version="1.0" encoding="UTF-8"?>
        <Response>
          <Endpoint>new-endpoint</Endpoint>
        </Response>
      )"));

  auto item = std::make_shared<Item>("id/");
  item->set_type(IItem::FileType::Directory);

  ExpectImmediatePromise(provider->createDirectory(item, "new_directory"), _);
}

TEST(AmazonS3Test, FiguresOutEndpointWhenRegionRequestFails) {
  auto mock = CloudFactoryMock::create();
  ICloudFactory::ProviderInitData data;
  data.token_ = util::encode_token(R"js({
                                          "bucket": "bucket",
                                          "endpoint": "endpoint",
                                          "username": "lemourindrive",
                                          "password": "password"
                                        })js");
  auto provider = mock.factory()->create("amazons3", GetDefaultInitData());

  ExpectHttp(mock.http(), "endpoint/bucket/id/new_directory/")
      .WithMethod("PUT")
      .WillRespondWithCode(401);

  ExpectHttp(mock.http(), "new-endpoint/id/new_directory/")
      .WithMethod("PUT")
      .WillRespondWithCode(200);

  ExpectHttp(mock.http(), "endpoint/bucket/")
      .WillRespondWith(HttpResponse().WithStatus(301).WithContent(R"(
        <?xml version="1.0" encoding="UTF-8"?>
        <Response>
          <Endpoint>new-endpoint</Endpoint>
        </Response>
      )"));

  ExpectHttp(mock.http(), "new-endpoint/")
      .WillRespondWith(R"(
        <?xml version="1.0" encoding="UTF-8"?>
        <Location>location</Location>
      )")
      .WillRespondWith(R"(
        <?xml version="1.0" encoding="UTF-8"?>
        <Response>
          <Endpoint>new-endpoint</Endpoint>
        </Response>
      )");

  auto item = std::make_shared<Item>("id/");
  item->set_type(IItem::FileType::Directory);

  ExpectImmediatePromise(provider->createDirectory(item, "new_directory"), _);
}

TEST(AmazonS3Test, GetsToken) {
  auto mock = CloudFactoryMock::create();
  ICloudFactory::ProviderInitData data;
  data.token_ = util::encode_token(R"js({
                                          "bucket": "bucket",
                                          "endpoint": "endpoint",
                                          "username": "lemourindrive",
                                          "password": "password"
                                        })js");
  auto provider = mock.factory()->create("amazons3", GetDefaultInitData());

  auto json = util::json::from_string(util::decode_token(provider->token()));
  EXPECT_EQ(json["bucket"], "bucket");
  EXPECT_EQ(json["endpoint"], "endpoint");
  EXPECT_EQ(json["username"], "lemourindrive");
  EXPECT_EQ(json["password"], "password");
}

}  // namespace cloudstorage
