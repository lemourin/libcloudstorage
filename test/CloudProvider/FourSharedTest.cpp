#include <Utility/Utility.h>
#include "Utility/CloudFactoryMock.h"
#include "Utility/Item.h"

namespace cloudstorage {

using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;
using ::testing::SizeIs;
using ::testing::StrEq;

TEST(FourSharedTest, GetsGeneralData) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("4shared", {});

  ExpectHttp(mock.http(), "https://api.4shared.com/v1_2/user")
      .WillRespondWith(R"js({
                              "email": "admin@admin.ru",
                              "totalSpace": 10,
                              "freeSpace": 1
                            })js");

  ExpectImmediatePromise(provider->generalData(),
                         AllOf(Field(&GeneralData::username_, "admin@admin.ru"),
                               Field(&GeneralData::space_total_, 10),
                               Field(&GeneralData::space_used_, 9)));
}

TEST(FourSharedTest, GetsItemDataForDirectory) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("4shared", {});

  ExpectHttp(mock.http(), "https://api.4shared.com/v1_2/folders/file_id")
      .WillRespondWith(R"js({
                              "name": "filename",
                              "id": "file_id",
                              "modified": "2012-04-12T04:06:12.4213Z",
                              "folderLink": "link"
                            })js");

  ExpectImmediatePromise(
      provider->getItemData(util::FileId(true, "file_id")),
      Pointee(
          AllOf(Property(&IItem::filename, "filename"),
                Property(&IItem::id, util::FileId(true, "file_id")),
                Property(&IItem::size, IItem::UnknownSize),
                Property(&IItem::type, IItem::FileType::Directory),
                Property(&IItem::timestamp,
                         std::chrono::system_clock::from_time_t(1334203572)))));
}

TEST(FourSharedTest, GetsItemDataForItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("4shared", {});

  ExpectHttp(mock.http(), "https://api.4shared.com/v1_2/files/file_id")
      .WillRespondWith(R"js({
                              "name": "filename",
                              "id": "file_id",
                              "size": 1234,
                              "modified": "2012-04-12T04:06:12.4213Z",
                              "mimeType": "image/jpg"
                            })js");

  ExpectImmediatePromise(
      provider->getItemData(util::FileId(false, "file_id")),
      Pointee(
          AllOf(Property(&IItem::filename, "filename"),
                Property(&IItem::id, util::FileId(false, "file_id")),
                Property(&IItem::size, 1234),
                Property(&IItem::type, IItem::FileType::Image),
                Property(&IItem::timestamp,
                         std::chrono::system_clock::from_time_t(1334203572)))));
}

TEST(FourSharedTest, ListsDirectory) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("4shared", {});

  ExpectHttp(mock.http(),
             "https://api.4shared.com/v1_2/folders/folder_id/children")
      .WillRespondWith(R"({
                            "folders": [{}]
                          })");
  ExpectHttp(mock.http(),
             "https://api.4shared.com/v1_2/folders/folder_id/files")
      .WillRespondWith(R"({
                            "files": [{}, {}]
                          })");

  auto directory = std::make_shared<Item>(
      "filename", util::FileId(true, "folder_id"), IItem::UnknownSize,
      IItem::UnknownTimeStamp, IItem::FileType::Directory);

  ExpectImmediatePromise(provider->listDirectory(directory), SizeIs(3));
}

TEST(FourSharedTest, DeletesItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("4shared", {});

  ExpectHttp(mock.http(), "https://api.4shared.com/v1_2/files/file_id")
      .WithMethod("DELETE")
      .WillRespondWithCode(200);

  auto item = std::make_shared<Item>(
      "filename", util::FileId(false, "file_id"), IItem::UnknownSize,
      IItem::UnknownTimeStamp, IItem::FileType::Unknown);

  ExpectImmediatePromise(provider->deleteItem(item));
}

TEST(FourSharedTest, DeletesFolder) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("4shared", {});

  ExpectHttp(mock.http(), "https://api.4shared.com/v1_2/folders/folder_id")
      .WithMethod("DELETE")
      .WillRespondWithCode(200);

  auto item = std::make_shared<Item>(
      "folder", util::FileId(true, "folder_id"), IItem::UnknownSize,
      IItem::UnknownTimeStamp, IItem::FileType::Directory);

  ExpectImmediatePromise(provider->deleteItem(item));
}

TEST(FourSharedTest, CreatesDirectory) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("4shared", {});

  ExpectHttp(mock.http(), "https://api.4shared.com/v1_2/folders")
      .WithMethod("POST")
      .WithHeaderParameter("Authorization", _)
      .WithHeaderParameter("cookie", _)
      .WithHeaderParameter("Content-Type", "application/x-www-form-urlencoded")
      .WithBody("parentId=folder_id&name=directory")
      .WillRespondWith(
          R"js({ "name": "directory", "folderLink": "folder" })js");

  auto parent = std::make_shared<Item>(
      "parent", util::FileId(true, "folder_id"), IItem::UnknownSize,
      IItem::UnknownTimeStamp, IItem::FileType::Directory);

  ExpectImmediatePromise(
      provider->createDirectory(parent, "directory"),
      Pointee(AllOf(Property(&IItem::filename, "directory"),
                    Property(&IItem::type, IItem::FileType::Directory))));
}

TEST(FourSharedTest, MovesItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("4shared", {});

  ExpectHttp(mock.http(), "https://api.4shared.com/v1_2/files/source_id/move")
      .WithMethod("POST")
      .WithHeaderParameter("Authorization", _)
      .WithHeaderParameter("cookie", _)
      .WithHeaderParameter("Content-Type", "application/x-www-form-urlencoded")
      .WithBody("folderId=destination_id")
      .WillRespondWith(R"js({ "name": "filename" })js");

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

TEST(FourSharedTest, MovesFolder) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("4shared", {});

  ExpectHttp(mock.http(), "https://api.4shared.com/v1_2/folders/source_id/move")
      .WithMethod("POST")
      .WithHeaderParameter("Authorization", _)
      .WithHeaderParameter("cookie", _)
      .WithHeaderParameter("Content-Type", "application/x-www-form-urlencoded")
      .WithBody("folderId=destination_id")
      .WillRespondWith(R"js({ "name": "filename", "folderLink": "link" })js");

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

TEST(FourSharedTest, RenamesItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("4shared", {});

  ExpectHttp(mock.http(), "https://api.4shared.com/v1_2/files/source_id")
      .WithMethod("PUT")
      .WithHeaderParameter("Authorization", _)
      .WithHeaderParameter("cookie", _)
      .WithHeaderParameter("Content-Type", "application/x-www-form-urlencoded")
      .WithBody("name=new_name")
      .WillRespondWith(R"js({ "name": "new_name" })js");

  auto item = std::make_shared<Item>(
      "source", util::FileId(false, "source_id"), IItem::UnknownSize,
      IItem::UnknownTimeStamp, IItem::FileType::Unknown);

  ExpectImmediatePromise(provider->renameItem(item, "new_name"),
                         Pointee(Property(&IItem::filename, "new_name")));
}

TEST(FourSharedTest, RenamesFolder) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("4shared", {});

  ExpectHttp(mock.http(), "https://api.4shared.com/v1_2/folders/source_id")
      .WithMethod("PUT")
      .WithHeaderParameter("Authorization", _)
      .WithHeaderParameter("cookie", _)
      .WithHeaderParameter("Content-Type", "application/x-www-form-urlencoded")
      .WithBody("name=new_name")
      .WillRespondWith(R"js({ "name": "new_name", "folderLink": "link" })js");

  auto item = std::make_shared<Item>(
      "source", util::FileId(true, "source_id"), IItem::UnknownSize,
      IItem::UnknownTimeStamp, IItem::FileType::Directory);

  ExpectImmediatePromise(provider->renameItem(item, "new_name"),
                         Pointee(Property(&IItem::filename, "new_name")));
}

TEST(FourSharedTest, GetsItemUrl) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("4shared", {});

  ExpectHttp(mock.http(), "https://api.4shared.com/v1_2/files/id")
      .WithMethod("GET")
      .WillRespondWith(R"js({ "downloadPage": "download-page" })js");

  ExpectHttp(mock.http(), "download-page")
      .WithMethod("HEAD")
      .WithFollowNoRedirect()
      .WillRespondWith(HttpResponse().WithHeaders(
          {{"location", "redirected-page/api/item-path"}}));

  ExpectHttp(mock.http(), "redirected-page/api/item-path")
      .WithMethod("HEAD")
      .WithFollowNoRedirect()
      .WillRespondWith(HttpResponse().WithHeaders(
          {{"set-cookie",
            "some_cookie=value; cd1v=cookie_value; tracking=value;"}}));

  ExpectHttp(mock.http(), "https://www.4shared.com/get//item-path")
      .WithFollowNoRedirect()
      .WithHeaderParameter("cookie", "cd1v=cookie_value;")
      .WillRespondWith(R"(<a id="baseDownloadLink" value="item-link"/>)");

  ExpectHttp(mock.http(), "item-link")
      .WithMethod("HEAD")
      .WillRespondWithCode(200);

  auto item = std::make_shared<Item>(
      "item", util::FileId(false, "id"), IItem::UnknownSize,
      IItem::UnknownTimeStamp, IItem::FileType::Unknown);

  ExpectImmediatePromise(provider->getFileUrl(item), StrEq("item-link"));
}

TEST(FourSharedTest, DownloadsItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("box", {});

  ExpectHttp(mock.http(), "https://api.box.com/2.0/files/id/content")
      .WithMethod("GET")
      .WillRespondWith("content");

  auto item = std::make_shared<Item>(
      "filename", util::FileId(false, "id"), IItem::UnknownSize,
      IItem::UnknownTimeStamp, IItem::FileType::Unknown);

  auto stream = std::make_shared<std::stringstream>();
  ExpectImmediatePromise(provider->downloadFile(
      item, FullRange, provider->streamDownloader(stream)));

  EXPECT_EQ(stream->str(), "content");
}

TEST(FourSharedTest, UploadsItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("4shared", {});

  ExpectHttp(mock.http(), "https://upload.4shared.com/v1_2/files")
      .WithMethod("POST")
      .WithHeaderParameter("Content-Type", "application/octet-stream")
      .WithHeaderParameter("Authorization", _)
      .WithHeaderParameter("cookie", _)
      .WithParameter("folderId", "id")
      .WithParameter("fileName", "name")
      .WithBody("content")
      .WillRespondWith(R"({ "name": "name" })");

  auto parent = std::make_shared<Item>(
      "filename", util::FileId(true, "id"), IItem::UnknownSize,
      IItem::UnknownTimeStamp, IItem::FileType::Unknown);

  auto stream = std::make_shared<std::stringstream>();
  *stream << "content";
  ExpectImmediatePromise(
      provider->uploadFile(parent, "name", provider->streamUploader(stream)),
      Pointee(Property(&IItem::filename, "name")));
}

TEST(FourSharedTest, ExchangesAuthorizationCode) {
  auto mock = CloudFactoryMock::create();

  ExpectHttp(mock.http(), "https://www.4shared.com/web/login")
      .WithMethod("POST")
      .WithFollowNoRedirect()
      .WithBody("login=username&password=password&remember=true")
      .WillRespondWith(HttpResponse().WithHeaders(
          {{"set-cookie", "Login=cookie-login; Password=cookie-password"}}));

  Json::Value json;
  json["username"] = "cookie-login";
  json["password"] = "cookie-password";

  ExpectImmediatePromise(
      mock.factory()->exchangeAuthorizationCode(
          "4shared", {}, util::encode_token(R"js({
                                                   "username": "username",
                                                   "password": "password"
                                                 })js")),
      Field(&Token::token_, util::encode_token(util::json::to_string(json))));
}

TEST(FourSharedTest, StoresToken) {
  auto mock = CloudFactoryMock::create();
  ICloudFactory::ProviderInitData data;
  data.token_ = util::encode_token(
      R"js({ "username": "username", "password": "password" })js");
  auto provider = mock.factory()->create("4shared", data);

  Json::Value json;
  json["username"] = "username";
  json["password"] = "password";

  EXPECT_EQ(provider->token(), util::encode_token(util::json::to_string(json)));
}

TEST(FourSharedTest, DoesAuthorization) {
  auto mock = CloudFactoryMock::create();
  ICloudFactory::ProviderInitData data;
  data.hints_["client_id"] = "client_id";
  data.hints_["client_secret"] = "client_secret";
  data.token_ = util::encode_token(
      R"js({ "username": "username", "password": "password" })js");
  auto provider = mock.factory()->create("4shared", data);

  ExpectHttp(mock.http(), "https://api.4shared.com/v1_2/user")
      .WillRespondWithCode(401)
      .WillRespondWith(R"js({ "rootFolderId": "rootFolderId" })js")
      .WillRespondWith(R"({})");

  ExpectHttp(mock.http(), "https://api.4shared.com/v1_2/oauth/initiate")
      .WithMethod("POST")
      .WillRespondWith(
          "oauth_token=oauth_token&oauth_token_secret=oauth_token_secret");

  ExpectHttp(mock.http(), "https://api.4shared.com/v1_2/oauth/authorize")
      .WithFollowNoRedirect()
      .WithHeaderParameter("cookie", "Login=username; Password=password")
      .WithParameter("oauth_token", "oauth_token")
      .WillRespondWith(
          HttpResponse()
              .WithHeaders({{"set-cookie", "oauth_session=oauth_session;"}})
              .WithContent(
                  R"(<a "oauth_authenticity_token" value="oauth_authenticity_token"/>)"));

  ExpectHttp(mock.http(), "https://api.4shared.com/v1_2/oauth/authorize")
      .WithMethod("POST")
      .WithFollowNoRedirect()
      .WithHeaderParameter(
          "cookie",
          "Login=username; Password=password; oauth_session=oauth_session;")
      .WithBody(
          R"(oauth_authenticity_token=oauth_authenticity_token&oauth_token=oauth_token&oauth_callback=http%3A%2F%2Flocalhost&allow=true)")
      .WillRespondWithCode(200);

  ExpectHttp(mock.http(), "https://api.4shared.com/v1_2/oauth/token")
      .WithMethod("POST")
      .WillRespondWith(
          "oauth_token=oauth_token&oauth_token_secret=oauth_token_secret");

  ExpectImmediatePromise(provider->generalData(), _);

  auto hints = provider->hints();

  EXPECT_EQ(hints["root_id"], "rootFolderId");
  EXPECT_EQ(hints["oauth_token"], "oauth_token");
  EXPECT_EQ(hints["oauth_token_secret"], "oauth_token_secret");
}

}  // namespace cloudstorage
