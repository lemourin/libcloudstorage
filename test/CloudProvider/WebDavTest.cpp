#include "gtest/gtest.h"

#include "Utility/CloudFactoryMock.h"
#include "Utility/Item.h"
#include "Utility/Utility.h"

namespace cloudstorage {

using testing::_;
using testing::AllOf;
using testing::ElementsAre;
using testing::Field;
using testing::Invoke;
using testing::InvokeArgument;
using testing::Pointee;
using testing::Property;
using testing::Return;
using testing::WithArgs;

TEST(WebDavTest, ListsDirectory) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("webdav", {});

  auto mock_response = Response(R"(
<?xml version='1.0' encoding='UTF-8'?>
<d:multistatus xmlns:d="DAV:">
    <d:response/>
    <d:response>
      <d:href>/Applications/</d:href>
      <d:propstat>
          <d:status>HTTP/1.1 200 OK</d:status>
          <d:prop>
              <d:creationdate>2016-07-14T13:38:36Z</d:creationdate>
              <d:displayname>Applications</d:displayname>
              <d:getlastmodified>Thu, 14 Jul 2016 13:38:36 GMT</d:getlastmodified>
              <d:resourcetype>
                  <d:collection/>
              </d:resourcetype>
          </d:prop>
      </d:propstat>
    </d:response>
</d:multistatus>
)");
  EXPECT_CALL(*mock_response, setHeaderParameter("Authorization", _));
  EXPECT_CALL(*mock_response, setHeaderParameter("Depth", "1"));
  EXPECT_CALL(*mock.http(), create("/", "PROPFIND", true))
      .WillOnce(Return(mock_response));

  ExpectImmediatePromise(
      provider->listDirectory(provider->root()),
      ElementsAre(Pointee(AllOf(
          Property(&IItem::id, "/Applications/"),
          Property(&IItem::filename, "Applications"),
          Property(&IItem::type, IItem::FileType::Directory),
          Property(&IItem::timestamp,
                   std::chrono::system_clock::from_time_t(1468503516))))));
}

TEST(WebDavTest, GetsGeneralData) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("webdav", {});

  auto mock_response = Response(
      R"(
<?xml version="1.0" encoding="UTF-8"?>
<d:multistatus xmlns:d="DAV:">
    <d:response>
        <d:href>/</d:href>
        <d:propstat>
            <d:status>HTTP/1.1 200 OK</d:status>
            <d:prop>
                <d:quota-used-bytes>861464169</d:quota-used-bytes>
                <d:quota-available-bytes>9875954071</d:quota-available-bytes>
            </d:prop>
        </d:propstat>
    </d:response>
</d:multistatus>
)",
      IgnoringWhitespace(R"(
<D:propfind xmlns:D="DAV:">
    <D:prop>
        <D:quota-available-bytes/>
        <D:quota-used-bytes/>
    </D:prop>
</D:propfind>
)"));
  EXPECT_CALL(*mock_response, setHeaderParameter("Depth", "0"));
  EXPECT_CALL(*mock_response, setHeaderParameter("Content-Type", "text/xml"));
  EXPECT_CALL(*mock_response, setHeaderParameter("Authorization", _));
  EXPECT_CALL(*mock.http(), create).WillOnce(Return(mock_response));

  ExpectImmediatePromise(provider->generalData(),
                         AllOf(Field(&GeneralData::space_used_, 861464169),
                               Field(&GeneralData::space_total_, 9875954071)));
}

TEST(WebDavTest, GetsItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("webdav", {});

  auto mock_response = Response(
      R"(
<?xml version='1.0' encoding='UTF-8'?>
<d:multistatus xmlns:d="DAV:">
    <d:response>
      <d:href>/Applications/</d:href>
      <d:propstat>
          <d:status>HTTP/1.1 200 OK</d:status>
          <d:prop>
              <d:creationdate>2016-07-14T13:38:36Z</d:creationdate>
              <d:displayname>Applications</d:displayname>
              <d:getlastmodified>Thu, 14 Jul 2016 13:38:36 GMT</d:getlastmodified>
              <d:resourcetype>
                  <d:collection/>
              </d:resourcetype>
          </d:prop>
      </d:propstat>
    </d:response>
</d:multistatus>
)");

  EXPECT_CALL(*mock_response, setHeaderParameter("Depth", "0"));
  EXPECT_CALL(*mock_response, setHeaderParameter("Authorization", _));
  EXPECT_CALL(*mock.http(), create).WillOnce(Return(mock_response));

  ExpectImmediatePromise(
      provider->getItemData("/Application/"),
      Pointee(
          AllOf(Property(&IItem::id, "/Applications/"),
                Property(&IItem::filename, "Applications"),
                Property(&IItem::type, IItem::FileType::Directory),
                Property(&IItem::timestamp,
                         std::chrono::system_clock::from_time_t(1468503516)))));
}

TEST(WebDavTest, CreatesDirectory) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("webdav", {});

  auto mock_response = Response("");

  EXPECT_CALL(*mock.http(), create("/parent/child/", "MKCOL", true))
      .WillOnce(Return(mock_response));

  ExpectImmediatePromise(
      provider->createDirectory(
          std::make_unique<Item>("parent", "/parent/", IItem::UnknownSize,
                                 IItem::UnknownTimeStamp,
                                 IItem::FileType::Directory),
          "child"),
      Pointee(AllOf(Property(&IItem::id, "/parent/child/"),
                    Property(&IItem::type, IItem::FileType::Directory),
                    Property(&IItem::size, 0))));
}

TEST(WebDavTest, DeletesItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("webdav", {});

  auto mock_response = Response("");

  EXPECT_CALL(*mock.http(), create("/parent/child/", "DELETE", true))
      .WillOnce(Return(mock_response));

  ExpectImmediatePromise(provider->deleteItem(std::make_unique<Item>(
      "child", "/parent/child/", IItem::UnknownSize, IItem::UnknownTimeStamp,
      IItem::FileType::Directory)));
}

TEST(WebDavTest, RenamesItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("webdav", {});

  auto mock_response = Response("");
  EXPECT_CALL(*mock_response,
              setHeaderParameter("Destination", "/parent/new_name"));
  EXPECT_CALL(*mock_response, setHeaderParameter("Authorization", _));

  EXPECT_CALL(*mock.http(), create("/parent/child", "MOVE", true))
      .WillOnce(Return(mock_response));

  ExpectImmediatePromise(
      provider->renameItem(
          std::make_unique<Item>("child", "/parent/child", IItem::UnknownSize,
                                 IItem::UnknownTimeStamp,
                                 IItem::FileType::Directory),
          "new_name"),
      Pointee(AllOf(Property(&IItem::id, "/parent/new_name"),
                    Property(&IItem::filename, "new_name"))));
}

TEST(WebDavTest, MovesItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("webdav", {});

  auto mock_response = Response("");
  EXPECT_CALL(*mock_response, setHeaderParameter("Destination", "/child"));
  EXPECT_CALL(*mock_response, setHeaderParameter("Authorization", _));

  EXPECT_CALL(*mock.http(), create("/parent/child/", "MOVE", true))
      .WillOnce(Return(mock_response));

  ExpectImmediatePromise(
      provider->moveItem(
          std::make_unique<Item>("child", "/parent/child/", IItem::UnknownSize,
                                 IItem::UnknownTimeStamp,
                                 IItem::FileType::Directory),
          provider->root()),
      Pointee(AllOf(Property(&IItem::id, "/child"))));
}

TEST(WebDavTest, UploadsFile) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("webdav", {});

  auto mock_response = Response("", "file");

  EXPECT_CALL(*mock.http(), create("/destination/filename", "PUT", true))
      .WillOnce(Return(mock_response));

  auto content = std::make_shared<std::stringstream>();
  *content << "file";
  ExpectImmediatePromise(
      provider->uploadFile(std::make_unique<Item>("child", "/destination/", 4,
                                                  IItem::UnknownTimeStamp,
                                                  IItem::FileType::Directory),
                           "filename", provider->streamUploader(content)),
      Pointee(AllOf(Property(&IItem::filename, "filename"),
                    Property(&IItem::size, 4))));
}

TEST(WebDavTest, DownloadsFile) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("webdav", {});

  auto mock_response = Response("file");

  EXPECT_CALL(*mock.http(), create("/url", "GET", true))
      .WillOnce(Return(mock_response));

  auto item =
      std::make_shared<Item>("file", "/file", IItem::UnknownSize,
                             IItem::UnknownTimeStamp, IItem::FileType::Unknown);
  item->set_url("/url");

  auto content = std::make_shared<std::stringstream>();
  ExpectImmediatePromise(provider->downloadFile(
      item, FullRange, provider->streamDownloader(content)));

  EXPECT_THAT(content->str(), "file");
}

TEST(WebDavTest, ParsesToken) {
  auto mock = CloudFactoryMock::create();
  auto provider =
      mock.factory()->create("webdav", {util::to_base64(util::Url::escape(R"({
  "username": "username",
  "password": "password",
  "endpoint": "endpoint"
})"))});

  auto json = util::json::from_string(
      util::Url::unescape(util::from_base64(provider->token())));
  EXPECT_EQ(json["username"], "username");
  EXPECT_EQ(json["password"], "password");
  EXPECT_EQ(json["endpoint"], "endpoint");
}

TEST(WebDavTest, HandlesAuthFailure) {
  auto mock = CloudFactoryMock::create();
  auto provider =
      mock.factory()->create("webdav", {util::to_base64(util::Url::escape(R"({
  "username": []
})"))});

  EXPECT_CALL(*mock.http(), create).WillOnce(Return(Response(401, "")));

  ExpectFailedPromise(provider->generalData(), Field(&Error::code_, 401));
}

}  // namespace cloudstorage
