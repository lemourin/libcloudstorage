#include "gtest/gtest.h"

#include "Utility/CloudFactoryMock.h"

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

}  // namespace cloudstorage
