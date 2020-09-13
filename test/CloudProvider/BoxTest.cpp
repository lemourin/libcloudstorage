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

TEST(BoxTest, GetsGeneralData) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("box", {});

  EXPECT_CALL(*mock.http(),
              create("https://api.box.com/2.0/users/me", "GET", true))
      .WillOnce(Return(MockResponse(R"js({
                                           "login": "admin@admin.ru",
                                           "space_amount": 10,
                                           "space_used": 5
                                         })js")));

  ExpectImmediatePromise(provider->generalData(),
                         AllOf(Field(&GeneralData::username_, "admin@admin.ru"),
                               Field(&GeneralData::space_total_, 10),
                               Field(&GeneralData::space_used_, 5)));
}

TEST(BoxTest, GetsItemDataForDirectory) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("box", {});

  auto response = MockResponse(R"js({
                                      "name": "filename",
                                      "id": "file_id",
                                      "size": 1234,
                                      "type": "folder",
                                      "modified_at": "2012-04-12T04:06:12.4213Z"
                                    })js");

  EXPECT_CALL(*mock.http(),
              create("https://api.box.com/2.0/folders/file_id", "GET", true))
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

TEST(BoxTest, GetsItemDataForItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("box", {});

  auto response = MockResponse(R"js({
                                      "name": "filename",
                                      "id": "file_id",
                                      "size": 1234,
                                      "modified_at": "2012-04-12T04:06:12.4213Z"
                                    })js");

  EXPECT_CALL(*mock.http(),
              create("https://api.box.com/2.0/files/file_id", "GET", true))
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

TEST(BoxTest, ListsDirectory) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("box", {});

  auto first_page_response = MockResponse(
      R"js({
             "entries":
             [ {}], "offset": 0, "limit": 1, "total_count": 5
           })js");
  EXPECT_CALL(*first_page_response,
              setParameter("fields", "name,id,size,modified_at"));

  auto second_page_response = MockResponse(R"({
                          "entries": [{}, {}, {}, {}],
                          "offset": 1,
                          "limit": 4,
                          "total_count": 5
                        })");
  EXPECT_CALL(*second_page_response,
              setParameter("fields", "name,id,size,modified_at"));
  EXPECT_CALL(*second_page_response, setParameter("offset", "1"));

  EXPECT_CALL(
      *mock.http(),
      create("https://api.box.com/2.0/folders/folder_id/items/", "GET", true))
      .WillOnce(Return(first_page_response))
      .WillOnce(Return(second_page_response));

  auto directory = std::make_shared<Item>(
      "filename", util::FileId(true, "folder_id"), IItem::UnknownSize,
      IItem::UnknownTimeStamp, IItem::FileType::Directory);

  ExpectImmediatePromise(provider->listDirectory(directory), SizeIs(5));
}

TEST(BoxTest, DeletesItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("box", {});

  EXPECT_CALL(*mock.http(),
              create("https://api.box.com/2.0/files/file_id", "DELETE", true))
      .WillOnce(Return(MockResponse(200)));

  auto item = std::make_shared<Item>(
      "filename", util::FileId(false, "file_id"), IItem::UnknownSize,
      IItem::UnknownTimeStamp, IItem::FileType::Unknown);

  ExpectImmediatePromise(provider->deleteItem(item));
}

TEST(BoxTest, DeletesFolder) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("box", {});

  auto response = MockResponse(200);
  EXPECT_CALL(*response, setParameter("recursive", "true"));

  EXPECT_CALL(*mock.http(), create("https://api.box.com/2.0/folders/folder_id",
                                   "DELETE", true))
      .WillOnce(Return(response));

  auto item = std::make_shared<Item>(
      "folder", util::FileId(true, "folder_id"), IItem::UnknownSize,
      IItem::UnknownTimeStamp, IItem::FileType::Directory);

  ExpectImmediatePromise(provider->deleteItem(item));
}

TEST(BoxTest, CreatesDirectory) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("box", {});

  auto response =
      MockResponse(R"js({ "name": "directory", "type": "folder" })js",
                   IgnoringWhitespace(R"js({
                                             "name": "directory",
                                             "parent": { "id": "folder_id" }
                                           })js"));
  EXPECT_CALL(*response, setHeaderParameter("Authorization", _));
  EXPECT_CALL(*response,
              setHeaderParameter("Content-Type", "application/json"));
  EXPECT_CALL(*mock.http(),
              create("https://api.box.com/2.0/folders", "POST", true))
      .WillOnce(Return(response));

  auto parent = std::make_shared<Item>(
      "parent", util::FileId(true, "folder_id"), IItem::UnknownSize,
      IItem::UnknownTimeStamp, IItem::FileType::Directory);

  ExpectImmediatePromise(
      provider->createDirectory(parent, "directory"),
      Pointee(AllOf(Property(&IItem::filename, "directory"),
                    Property(&IItem::type, IItem::FileType::Directory))));
}

TEST(BoxTest, MovesItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("box", {});

  auto response = MockResponse(
      R"js({ "name": "filename" })js",
      IgnoringWhitespace(R"js({ "parent": { "id": "destination_id" } })js"));
  EXPECT_CALL(*response, setHeaderParameter("Authorization", _));
  EXPECT_CALL(*response,
              setHeaderParameter("Content-Type", "application/json"));
  EXPECT_CALL(*mock.http(),
              create("https://api.box.com/2.0/files/source_id", "PUT", true))
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

TEST(BoxTest, MovesFolder) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("box", {});

  auto response = MockResponse(
      R"js({ "name": "filename", "type": "folder" })js",
      IgnoringWhitespace(R"js({ "parent": { "id": "destination_id" } })js"));
  EXPECT_CALL(*response, setHeaderParameter("Authorization", _));
  EXPECT_CALL(*response,
              setHeaderParameter("Content-Type", "application/json"));
  EXPECT_CALL(*mock.http(),
              create("https://api.box.com/2.0/folders/source_id", "PUT", true))
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

TEST(BoxTest, RenamesItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("box", {});

  auto response =
      MockResponse(R"js({ "name": "new_name" })js",
                   IgnoringWhitespace(R"js({ "name": "new_name" })js"));

  EXPECT_CALL(*mock.http(),
              create("https://api.box.com/2.0/files/source_id", "PUT", true))
      .WillOnce(Return(response));

  auto item = std::make_shared<Item>(
      "source", util::FileId(false, "source_id"), IItem::UnknownSize,
      IItem::UnknownTimeStamp, IItem::FileType::Unknown);

  ExpectImmediatePromise(provider->renameItem(item, "new_name"),
                         Pointee(Property(&IItem::filename, "new_name")));
}

TEST(BoxTest, RenamesFolder) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("box", {});

  auto response =
      MockResponse(R"js({ "name": "new_name" })js",
                   IgnoringWhitespace(R"js({ "name": "new_name" })js"));

  EXPECT_CALL(*mock.http(),
              create("https://api.box.com/2.0/folders/source_id", "PUT", true))
      .WillOnce(Return(response));

  auto item = std::make_shared<Item>(
      "source", util::FileId(true, "source_id"), IItem::UnknownSize,
      IItem::UnknownTimeStamp, IItem::FileType::Directory);

  ExpectImmediatePromise(provider->renameItem(item, "new_name"),
                         Pointee(Property(&IItem::filename, "new_name")));
}

TEST(BoxTest, GetsThumbnail) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("box", {});

  EXPECT_CALL(
      *mock.http(),
      create("https://api.box.com/2.0/files/id/thumbnail.png", "GET", true))
      .WillOnce(Return(MockResponse("thumbnail")));

  auto source = std::make_shared<Item>(
      "source.jpg", util::FileId(false, "id"), IItem::UnknownSize,
      IItem::UnknownTimeStamp, IItem::FileType::Image);

  auto stream = std::make_shared<std::stringstream>();
  ExpectImmediatePromise(
      provider->generateThumbnail(source, provider->streamDownloader(stream)));

  EXPECT_EQ(stream->str(), "thumbnail");
}

TEST(BoxTest, GetsItemUrl) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("box", {});

  auto response = MockResponse(200, {{"location", "item-link"}}, "", "");
  EXPECT_CALL(*mock.http(),
              create("https://api.box.com/2.0/files/id/content", "GET", false))
      .WillRepeatedly(Return(response));
  EXPECT_CALL(*mock.http(), create("item-link", "HEAD", true))
      .WillOnce(Return(MockResponse(200)));

  auto item = std::make_shared<Item>(
      "item", util::FileId(false, "id"), IItem::UnknownSize,
      IItem::UnknownTimeStamp, IItem::FileType::Unknown);

  ExpectImmediatePromise(provider->getFileUrl(item), StrEq("item-link"));
}

TEST(BoxTest, DownloadsItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("box", {});

  EXPECT_CALL(*mock.http(),
              create("https://api.box.com/2.0/files/id/content", "GET", true))
      .WillOnce(Return(MockResponse("content")));

  auto item = std::make_shared<Item>(
      "filename", util::FileId(false, "id"), IItem::UnknownSize,
      IItem::UnknownTimeStamp, IItem::FileType::Unknown);

  auto stream = std::make_shared<std::stringstream>();
  ExpectImmediatePromise(provider->downloadFile(
      item, FullRange, provider->streamDownloader(stream)));

  EXPECT_EQ(stream->str(), "content");
}

TEST(BoxTest, UploadsItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("box", {});

  auto response = MockResponse(
      R"js({
             "entries":
             [ { "name": "name" }]
           })js",
      "--Thnlg1ecwyUJHyhYYGrQ\r\nContent-Disposition: form-data; "
      "name=\"attributes\"\\r\\n\\r\\n{\"name\":\"name\",\"parent\":{\"id\":"
      "\"id\"}}\r\n--Thnlg1ecwyUJHyhYYGrQ\r\nContent-Disposition: form-data; "
      "name=\"file\"; filename=\"\"name\"\"\\r\\nContent-Type: "
      "application/octet-stream\r\n\r\ncontent\r\n--Thnlg1ecwyUJHyhYYGrQ--");
  EXPECT_CALL(
      *response,
      setHeaderParameter("Content-Type",
                         "multipart/form-data; boundary=Thnlg1ecwyUJHyhYYGrQ"));
  EXPECT_CALL(*response, setHeaderParameter("Authorization", _));

  EXPECT_CALL(
      *mock.http(),
      create("https://upload.box.com/api/2.0/files/content", "POST", true))
      .WillOnce(Return(response));

  auto parent = std::make_shared<Item>(
      "filename", util::FileId(true, "id"), IItem::UnknownSize,
      IItem::UnknownTimeStamp, IItem::FileType::Unknown);

  auto stream = std::make_shared<std::stringstream>();
  *stream << "content";
  ExpectImmediatePromise(
      provider->uploadFile(parent, "name", provider->streamUploader(stream)),
      Pointee(Property(&IItem::filename, "name")));
}

TEST(BoxTest, RefreshesToken) {
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

TEST(BoxTest, ExchangesAuthorizationCode) {
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
      R"(grant_type=authorization_code&code=code&client_id=client_id&client_secret=client_secret)");

  EXPECT_CALL(*mock.http(),
              create("https://api.box.com/oauth2/token", "POST", true))
      .WillOnce(Return(MockResponse(200)))
      .WillOnce(Return(response));

  ExpectImmediatePromise(
      mock.factory()->exchangeAuthorizationCode("box", data, "code"),
      AllOf(Field(&Token::token_, "token"),
            Field(&Token::access_token_, "access_token")));
}

TEST(BoxTest, GetsAuthorizeLibraryUrl) {
  auto mock = CloudFactoryMock::create();
  ICloudFactory::ProviderInitData data;
  data.hints_["client_id"] = "client_id";
  data.hints_["client_secret"] = "client_secret";
  data.hints_["redirect_uri"] = "http://redirect-uri/";
  data.hints_["state"] = "state";

  EXPECT_EQ(
      mock.factory()->authorizationUrl("box", data),
      R"(https://account.box.com/api/oauth2/authorize?response_type=code&client_id=client_id&redirect_uri=http://redirect-uri/&state=state)");
}

TEST(BoxTest, RootUsesFileId) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("box", {});

  EXPECT_EQ(provider->root()->id(), std::string(util::FileId(true, "0")));
}

}  // namespace cloudstorage
