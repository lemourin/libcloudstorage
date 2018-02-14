/*****************************************************************************
 * CloudProviderTest.cpp
 *
 *****************************************************************************
 * Copyright (C) 2016-2016 VideoLAN
 *
 * Authors: Paweł Wegner <pawel.wegner95@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "gtest/gtest.h"

#include <json/json.h>
#include <fstream>
#include <future>
#include "CloudProvider/LocalDrive.h"
#include "ICloudProvider.h"
#include "ICloudStorage.h"
#include "Utility/Utility.h"

using namespace cloudstorage;

const int FILE_CNT = 2;

#ifdef TOKEN_FILE
#ifdef WITH_CURL

class TokenData {
 public:
  TokenData() {
    try {
      json_ = util::json::from_stream(std::fstream(TOKEN_FILE));
    } catch (const Json::Exception&) {
      util::log("invalid token file", TOKEN_FILE);
      std::terminate();
    }
  }

  std::string token(const std::string& name) const {
    return json_[name].asString();
  }

 private:
  Json::Value json_;

} token_data;

class UploadFile : public IUploadFileCallback {
 public:
  UploadFile(const std::string& str) { stream_ << str; }

  void reset() override { stream_.seekg(std::istream::beg); }

  uint32_t putData(char* data, uint32_t maxlength) override {
    stream_.read(data, maxlength);
    return stream_.gcount();
  }

  uint64_t size() override { return stream_.str().size(); }

  void done(EitherError<IItem>) override {}

  void progress(uint64_t, uint64_t) override {}

 private:
  std::stringstream stream_;
};

class DownloadFile : public cloudstorage::IDownloadFileCallback {
 public:
  void receivedData(const char* data, uint32_t length) override {
    stream_.write(data, length);
  }

  void done(cloudstorage::EitherError<void> e) override { result_ = e; }

  void progress(uint64_t, uint64_t) override {}

  std::string data() const { return stream_.str(); }

 private:
  std::stringstream stream_;
  EitherError<void> result_;
};

class CloudProviderTest : public ::testing::Test {
 public:
  static ICloudProvider::Pointer create(const char* name,
                                        const char* root_path) {
    class AuthCallback : public ICloudProvider::IAuthCallback {
     public:
      Status userConsentRequired(const ICloudProvider&) override {
        return Status::None;
      }

      void done(const ICloudProvider&, EitherError<void>) override {}
    };

    ICloudProvider::InitData data;
    data.callback_ = util::make_unique<AuthCallback>();
    data.token_ = token_data.token(name);
    auto p = ICloudStorage::create()->provider(name, std::move(data));
    initialize(p.get(), root_path);
    return p;
  }

  static void release(ICloudProvider* p) {
    auto json = util::json::from_stream(std::ifstream(TOKEN_FILE));
    json[p->name()] = p->token();
    std::ofstream(TOKEN_FILE) << json;
  }

  static void initialize(ICloudProvider* p, const std::string& path) {
    auto root = p->getItemAsync(path)->result().right();
    if (root != nullptr)
      ASSERT_EQ(p->deleteItemAsync(root)->result().left(), nullptr);
    auto it = path.find_last_of('/');
    ASSERT_NE(it, std::string::npos);
    auto parent_path = std::string(path.begin(), path.begin() + it + 1);
    auto name = std::string(path.begin() + it + 1, path.end());
    auto parent = p->getItemAsync(parent_path)->result().right();
    ASSERT_NE(parent, nullptr);
    auto item = p->createDirectoryAsync(parent, name)->result().right();
    ASSERT_NE(item, nullptr);
    for (int i = 0; i < FILE_CNT; i++) {
      auto directory =
          p->createDirectoryAsync(item, std::to_string(i))->result().right();
      ASSERT_NE(directory, nullptr);
      auto upload =
          p->uploadFileAsync(directory, std::to_string(i) + ".txt",
                             util::make_unique<UploadFile>("0123456789"))
              ->result();
      ASSERT_EQ(upload.left(), nullptr);
    }
  }

  static std::vector<std::string> filename(const IItem::List& lst) {
    std::vector<std::string> result;
    for (auto p : lst) result.push_back(p->filename());
    std::sort(result.begin(), result.end());
    return result;
  }

  static void lookup(ICloudProvider* p, IItem::Pointer directory,
                     const std::string& name, IItem::Pointer& result) {
    auto lst = p->listDirectoryAsync(directory)->result().right();
    ASSERT_NE(lst, nullptr);
    for (auto i : *lst)
      if (i->filename() == name) {
        result = i;
        return;
      }
    ASSERT_EQ("found", "not found");
  }

  static IItem::Pointer lookup(ICloudProvider* p, IItem::Pointer directory,
                               const std::string& name) {
    IItem::Pointer result;
    lookup(p, directory, name, result);
    return result;
  }

  static void rename_directory(ICloudProvider* p, const std::string& path) {
    auto root = p->getItemAsync(path)->result().right();
    ASSERT_NE(root, nullptr);
    auto item = p->getItemAsync(path + "/1")->result().right();
    ASSERT_NE(item, nullptr);
    item = p->renameItemAsync(item, "3")->result().right();
    ASSERT_NE(item, nullptr);
    auto lst = p->listDirectoryAsync(root)->result().right();
    ASSERT_NE(lst, nullptr);
    ASSERT_EQ(filename(*lst), std::vector<std::string>({"0", "3"}));
    ASSERT_NE(p->renameItemAsync(item, "1")->result().right(), nullptr);
    lst = p->listDirectoryAsync(root)->result().right();
    ASSERT_NE(lst, nullptr);
    ASSERT_EQ(filename(*lst), std::vector<std::string>({"0", "1"}));
  }

  static void rename_item(ICloudProvider* p, const std::string& path) {
    auto root = p->getItemAsync(path)->result().right();
    ASSERT_NE(root, nullptr);
    auto item = p->getItemAsync(path + "/0/0.txt")->result().right();
    ASSERT_NE(item, nullptr);
    item = p->renameItemAsync(item, "1.txt")->result().right();
    ASSERT_NE(item, nullptr);
    auto directory = lookup(p, root, "0");
    auto lst = p->listDirectoryAsync(directory)->result().right();
    ASSERT_NE(lst, nullptr);
    ASSERT_EQ(filename(*lst), std::vector<std::string>({"1.txt"}));
    ASSERT_NE(p->renameItemAsync(item, "0.txt")->result().right(), nullptr);
    lst = p->listDirectoryAsync(directory)->result().right();
    ASSERT_NE(lst, nullptr);
    ASSERT_EQ(filename(*lst), std::vector<std::string>({"0.txt"}));
  }

  static void move_directory(ICloudProvider* p, const std::string& path) {
    auto root = p->getItemAsync(path)->result().right();
    ASSERT_NE(root, nullptr);
    auto source = p->getItemAsync(path + "/0")->result().right();
    auto destination = p->getItemAsync(path + "/1")->result().right();
    ASSERT_NE(source, nullptr);
    ASSERT_NE(destination, nullptr);
    auto moved = p->moveItemAsync(source, destination)->result().right();
    ASSERT_NE(moved, nullptr);
    auto lst = p->listDirectoryAsync(root)->result().right();
    ASSERT_NE(lst, nullptr);
    ASSERT_EQ(filename(*lst), std::vector<std::string>({"1"}));
    lst = p->listDirectoryAsync(destination)->result().right();
    ASSERT_NE(lst, nullptr);
    ASSERT_EQ(filename(*lst), std::vector<std::string>({"0", "1.txt"}));
    lst = p->listDirectoryAsync(moved)->result().right();
    ASSERT_NE(lst, nullptr);
    ASSERT_EQ(filename(*lst), std::vector<std::string>({"0.txt"}));
    ASSERT_NE(p->moveItemAsync(moved, root)->result().right(), nullptr);
    lst = p->listDirectoryAsync(root)->result().right();
    ASSERT_NE(lst, nullptr);
    ASSERT_EQ(filename(*lst), std::vector<std::string>({"0", "1"}));
  }

  static void move_item(ICloudProvider* p, const std::string& path) {
    auto source_parent = p->getItemAsync(path + "/0")->result().right();
    auto source = p->getItemAsync(path + "/0/0.txt")->result().right();
    auto destination = p->getItemAsync(path + "/1")->result().right();
    ASSERT_NE(source_parent, nullptr);
    ASSERT_NE(source, nullptr);
    ASSERT_NE(destination, nullptr);
    auto moved = p->moveItemAsync(source, destination)->result().right();
    ASSERT_NE(moved, nullptr);
    auto lst = p->listDirectoryAsync(destination)->result().right();
    ASSERT_NE(lst, nullptr);
    ASSERT_EQ(filename(*lst), std::vector<std::string>({"0.txt", "1.txt"}));
    lst = p->listDirectoryAsync(source_parent)->result().right();
    ASSERT_NE(lst, nullptr);
    ASSERT_TRUE(filename(*lst).empty());
    ASSERT_NE(p->moveItemAsync(moved, source_parent)->result().right(),
              nullptr);
    lst = p->listDirectoryAsync(source_parent)->result().right();
    ASSERT_NE(lst, nullptr);
    ASSERT_EQ(filename(*lst), std::vector<std::string>({"0.txt"}));
  }

  static void read_item(ICloudProvider* p, const std::string& path) {
    auto item = p->getItemAsync(path + "/0/0.txt")->result().right();
    ASSERT_NE(item, nullptr);
    auto callback = std::make_shared<DownloadFile>();
    p->downloadFileAsync(item, callback)->result();
    ASSERT_EQ(callback->data(), "0123456789");
  }

  static void read_range(ICloudProvider* p, const std::string& path) {
    auto item = p->getItemAsync(path + "/1/1.txt")->result().right();
    ASSERT_NE(item, nullptr);
    auto callback = std::make_shared<DownloadFile>();
    p->downloadFileAsync(item, callback, Range{4, 2})->result();
    ASSERT_EQ(callback->data(), "45");
  }

  static void read_link(ICloudProvider* p, const std::string& path) {
    auto item = p->getItemAsync(path + "/0/0.txt")->result().right();
    ASSERT_NE(item, nullptr);
    auto lnk = p->getItemUrlAsync(item)->result().right();
    ASSERT_NE(lnk, nullptr);
    auto http = IHttp::create();
    auto request = http->create(*lnk, "HEAD", true);
    std::promise<IHttpRequest::Response> response_promise;
    request->send(
        [&](IHttpRequest::Response r) { response_promise.set_value(r); },
        std::make_shared<std::stringstream>(),
        std::make_shared<std::stringstream>());
    auto response = response_promise.get_future().get();
    ASSERT_TRUE(IHttpRequest::isSuccess(response.http_code_));
    auto length = response.headers_.find("content-length");
    ASSERT_NE(length, response.headers_.end());
    ASSERT_EQ(std::stoi(length->second), 10);
  }

  static void check_same(IItem::Pointer data, IItem::Pointer item) {
    ASSERT_EQ(data->filename(), item->filename());
    ASSERT_EQ(data->id(), item->id());
    ASSERT_EQ(data->type(), item->type());
    if (item->timestamp() != IItem::UnknownTimeStamp)
      ASSERT_EQ(data->timestamp(), item->timestamp());
    if (item->size() != IItem::UnknownSize)
      ASSERT_EQ(data->size(), item->size());
  }

  static void directory_data(ICloudProvider* p, const std::string& path) {
    auto item = p->getItemAsync(path + "/0")->result().right();
    ASSERT_NE(item, nullptr);
    auto data = p->getItemDataAsync(item->id())->result().right();
    ASSERT_NE(data, nullptr);
    check_same(data, item);
  }

  static void item_data(ICloudProvider* p, const std::string& path) {
    auto item = p->getItemAsync(path + "/0/0.txt")->result().right();
    ASSERT_NE(item, nullptr);
    auto data = p->getItemDataAsync(item->id())->result().right();
    ASSERT_NE(data, nullptr);
    check_same(data, item);
  }
};

#define CloudProvider(name, provider_name, path) \
  class name : public CloudProviderTest {        \
   public:                                       \
    static void SetUpTestCase() {                \
      path_ = path;                              \
      provider_ = create(provider_name, path);   \
    }                                            \
    static void TearDownTestCase() {             \
      release(provider_.get());                  \
      provider_ = nullptr;                       \
    }                                            \
                                                 \
    static ICloudProvider::Pointer provider_;    \
    static std::string path_;                    \
  };                                             \
  ICloudProvider::Pointer name::provider_;       \
  std::string name::path_

#define TEST_SET_F(name, provider_name, path)                                 \
  CloudProvider(name, provider_name, path);                                   \
                                                                              \
  TEST_F(name, RenameDirectory) { rename_directory(provider_.get(), path_); } \
                                                                              \
  TEST_F(name, RenameItem) { rename_item(provider_.get(), path_); }           \
                                                                              \
  TEST_F(name, MoveDirectory) { move_directory(provider_.get(), path_); }     \
                                                                              \
  TEST_F(name, MoveItem) { move_item(provider_.get(), path_); }               \
                                                                              \
  TEST_F(name, ReadItem) { read_item(provider_.get(), path_); }               \
                                                                              \
  TEST_F(name, ReadRange) { read_range(provider_.get(), path_); }             \
                                                                              \
  TEST_F(name, DirectoryData) { directory_data(provider_.get(), path_); }     \
                                                                              \
  TEST_F(name, ItemData) { item_data(provider_.get(), path_); }

#define TEST_SET(name, provider_name) TEST_SET_F(name, provider_name, "/root")

TEST_SET(GoogleProvider, "google")
TEST_SET(OneDriveProvider, "onedrive")
TEST_SET(DropboxProvider, "dropbox")
TEST_SET(HubiCProvider, "hubic")
TEST_SET(WebDAVProvider, "webdav")
TEST_SET(YandexProvider, "yandex")
TEST_SET(PCloudProvider, "pcloud")
TEST_SET(BoxProvider, "box")
TEST_SET(AmazonS3Provider, "amazons3")

#ifdef WITH_MEGA
TEST_SET(MegaProvider, "mega")
#endif  // WITH_MEGA

#ifdef WITH_LOCALDRIVE
TEST_SET(LocalDriveProvider, "local")
#endif  // WITH_LOCALDRIVE

#endif  // WITH_CURL
#endif  // TOKEN_FILE
