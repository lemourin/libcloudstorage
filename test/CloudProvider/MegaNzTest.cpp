#ifdef WITH_MEGA

#include "gtest/gtest.h"

#include <future>

#include "ICloudAccess.h"
#include "TestData.h"
#include "Utility/CloudFactoryMock.h"
#include "Utility/CloudProviderMock.h"
#include "Utility/Utility.h"

namespace cloudstorage {

using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::Invoke;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::StrEq;
using ::testing::WithArg;
using ::testing::WithArgs;

const std::string TOKEN(reinterpret_cast<const char*>(mega_token_txt),
                        mega_token_txt_len);

const std::string LOGIN_RESPONSE(
    reinterpret_cast<const char*>(mega_login_response_json),
    mega_login_response_json_len);

const std::string FILES_RESPONSE(
    reinterpret_cast<const char*>(mega_files_response_json),
    mega_files_response_json_len);

const uint8_t ENCODED_FILE[] = {
    0xcb, 0x4,  0x10, 0x6,  0x3,  0xb9, 0x47, 0x7,  0x14, 0xff, 0xeb, 0x5d,
    0x19, 0x17, 0xac, 0x26, 0x5d, 0x7c, 0x59, 0x28, 0x98, 0x92, 0xab, 0x34,
    0xb2, 0x77, 0xdb, 0xfa, 0xd1, 0xd2, 0x14, 0xcc, 0x37, 0x59, 0xb3, 0xdd,
    0xb3, 0x9e, 0xb,  0x70, 0x9,  0x93, 0x35, 0x94, 0xcd, 0x26, 0x4d, 0x98,
    0xd,  0x7a, 0xfb, 0xa4, 0xf2, 0x99, 0xa8, 0x94, 0x4c, 0xdf, 0xa5, 0xf1,
    0x1b, 0x95, 0x34, 0x1f, 0xe1, 0xa0, 0x52, 0x28, 0x48, 0x90, 0x37, 0x67,
    0x6a, 0xa2, 0xc8, 0xa0, 0x9,  0x79, 0x9b, 0xf3, 0x47, 0x60, 0x33, 0x3f,
    0x92, 0xaa, 0x5b, 0x3e, 0x3d, 0x4b, 0x9c, 0xd1, 0xd7, 0xc3, 0xb3, 0xf2,
    0x8b, 0xec, 0xa9, 0xb,  0xd1, 0xad, 0x9a, 0xd2, 0x7c, 0x19, 0x77, 0x1,
    0x6a, 0xdb, 0x38, 0xd,  0xc8, 0xb0, 0xbc, 0x40, 0x6b, 0x96, 0xa9, 0x34,
    0xbf, 0x51, 0xc1, 0x1b, 0x78, 0x25, 0xd2, 0x5f, 0x0,  0x3b, 0xbc, 0x93,
    0xef, 0xbb, 0x31, 0xff, 0x5f, 0xc8, 0x67, 0x4c, 0xb4, 0x91, 0xbb, 0xe4,
    0xb5, 0x76, 0x2,  0xcd, 0x23, 0x38, 0xd0, 0x15, 0x15, 0xd7, 0xe8, 0xbf,
    0x63, 0x1e, 0x5e, 0x14, 0xb1, 0x1a, 0xfd, 0x9a, 0x7a, 0xcb, 0xfb, 0x58,
    0x61, 0xb8, 0x96, 0xd4, 0x30, 0xf8, 0xc9, 0xe9, 0xca, 0x85, 0x5a, 0x1e,
    0xa2, 0x7d, 0xd5, 0x78, 0xb4, 0x70, 0xf0, 0xa0, 0x36, 0x87, 0x3d, 0xc7,
    0xaa, 0xa7, 0x32, 0x78, 0xf4, 0xd7};

std::shared_ptr<HttpRequestMock> MockedMegaResponse(const std::string& url) {
  auto request = std::make_shared<HttpRequestMock>();
  EXPECT_CALL(*request, send)
      .WillRepeatedly(DoAll(WithArgs<
                            0, 1, 2,
                            3>(Invoke([=](const IHttpRequest::CompleteCallback&
                                              on_completed,
                                          const std::shared_ptr<std::istream>&
                                              input_stream,
                                          const std::shared_ptr<std::ostream>&
                                              output_stream,
                                          const std::shared_ptr<std::ostream>&
                                              error_stream) {
        IHttpRequest::Response response;
        response.http_code_ = 200;
        response.output_stream_ = output_stream;
        response.error_stream_ = error_stream;

        std::stringstream sstream;
        sstream << input_stream->rdbuf();

        std::string output;

        if (url.find("https://g.api.mega.co.nz/cs") != std::string::npos) {
          auto json = util::json::from_string(sstream.str());
          if (json[0]["a"] == "us") {
            output = LOGIN_RESPONSE;
          } else if (json[0]["a"] == "f") {
            output = FILES_RESPONSE;
          } else if (json[0]["a"] == "log") {
            output = "[0]";
          } else if (json[0]["a"] == "uq") {
            output =
                R"([{"mstrg":53687091200,"cstrgn":{"UIZ2wIzS":[3210730732,424,7,8340824,1],"0Npw3ZzD":[0,0,0,0,0],"UEhzlbJA":[0,0,0,0,0]},"cstrg":3210730732,"uslw":9000,"srvratio":25.000381475547417}])";
          } else if (json[0]["a"] == "g") {
            output =
                R"([{"s":198,"at":"ftV2ak5v9cUqd9did04dD1Qt1DpoSuBPJ1T-0sl3DSlBvPlN5YOVc225bUUHq3FkeD34Sn5maTCD8sA35SgCow","msd":1,"tl":0,"g":"http://gfs270n375.userstorage.mega.co.nz/dl/90o5foCMRiTmrgIIHWIZDRKtrSy4EeJRCBJDstc85sOrmmzzE3u2mhYZVRr5R-RI82nJVevpiHvzCkefzqqx5xPifr_G85xdPJZKag1VboElBjI7ugqt_w","pfa":1}])";
          }
        } else if (
            url ==
            R"(https://g.api.mega.co.nz/wsc?sn=kT5h5dzLGbI&sid=3Lpm9xrLONJ2W7CwGN9LLEVkbkljbmdRU1ZNi4ck5FBxxE4E_d99A20_tA)") {
          output =
              R"({"w":"https://g.api.mega.co.nz/wsc/_pkb7Bs-cdSjYs6bB9s11Q","sn":"XqlH_atPIrU"})";
        } else if (
            url ==
            R"(https://g.api.mega.co.nz/wsc?c=50&sid=3Lpm9xrLONJ2W7CwGN9LLEVkbkljbmdRU1ZNi4ck5FBxxE4E_d99A20_tA)") {
          output = R"({"c":[],"lsn":"S0eXeaePFAA"})";
        } else if (
            url ==
            R"(https://g.api.mega.co.nz/wsc/_pkb7Bs-cdSjYs6bB9s11Q?sn=XqlH_atPIrU&sid=3Lpm9xrLONJ2W7CwGN9LLEVkbkljbmdRU1ZNi4ck5FBxxE4E_d99A20_tA)") {
          return;
        } else if (
            url ==
            R"(http://gfs270n375.userstorage.mega.co.nz/dl/90o5foCMRiTmrgIIHWIZDRKtrSy4EeJRCBJDstc85sOrmmzzE3u2mhYZVRr5R-RI82nJVevpiHvzCkefzqqx5xPifr_G85xdPJZKag1VboElBjI7ugqt_w/0-197)") {
          output = std::string(reinterpret_cast<const char*>(ENCODED_FILE),
                               sizeof(ENCODED_FILE));
        }

        ASSERT_FALSE(output.empty());

        response.headers_.insert(
            {"content-length", std::to_string(output.length())});

        *output_stream << output;

        on_completed(response);
      }))));
  return request;
}

TEST(MegaNzTest, ListsDirectory) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("mega", {TOKEN});

  EXPECT_CALL(*mock.http(), create)
      .WillRepeatedly(WithArg<0>(Invoke(MockedMegaResponse)));

  std::promise<EitherError<IItem::List>> semaphore;
  auto request =
      provider->listDirectory(provider->root())
          .then([&](const IItem::List& d) { semaphore.set_value(d); })
          .error<IException>([&](const auto& e) {
            semaphore.set_value(Error{e.code(), e.what()});
          });

  EitherError<IItem::List> result = semaphore.get_future().get();

  EXPECT_THAT(
      result.right(),
      Pointee(ElementsAre(
          Pointee(Property(&IItem::filename, "Pictures")),
          Pointee(Property(&IItem::filename, "Videos")),
          Pointee(Property(&IItem::filename, "Camera Uploads")),
          Pointee(Property(&IItem::filename, "Movies")),
          Pointee(Property(&IItem::filename, "Other")),
          Pointee(Property(&IItem::filename, "Music")),
          Pointee(Property(&IItem::filename, "Moje pliki czatu")),
          Pointee(Property(&IItem::filename,
                           "Bit Rush _ Login Screen - League of Legends.mp4")),
          Pointee(Property(&IItem::filename,
                           "STEINS;GATE 0 Ending Full LAST GAME by Zwei.mp4")),
          Pointee(Property(&IItem::filename, "docker_clean.sh")))));
}

TEST(MegaNzTest, GetsGeneralData) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("mega", {TOKEN});

  EXPECT_CALL(*mock.http(), create)
      .WillRepeatedly(WithArg<0>(Invoke(MockedMegaResponse)));

  std::promise<EitherError<GeneralData>> semaphore;
  auto request =
      provider->generalData()
          .then([&](const GeneralData& d) { semaphore.set_value(d); })
          .error<IException>([&](const auto& e) {
            semaphore.set_value(Error{e.code(), e.what()});
          });

  EitherError<GeneralData> result = semaphore.get_future().get();

  ASSERT_NE(result.right(), nullptr);

  EXPECT_EQ(result.right()->username_, "lemourin@gmail.com");
  EXPECT_EQ(result.right()->space_total_, 53687091200);
  EXPECT_EQ(result.right()->space_used_, 3210730732);
}

TEST(MegaNzTest, GetsItemData) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("mega", {TOKEN});

  EXPECT_CALL(*mock.http(), create)
      .WillRepeatedly(WithArg<0>(Invoke(MockedMegaResponse)));

  std::promise<EitherError<IItem>> semaphore;
  auto request =
      provider->getItemData("238628601250033")
          .then([&](const IItem::Pointer& d) { semaphore.set_value(d); })
          .error<IException>([&](const auto& e) {
            semaphore.set_value(Error{e.code(), e.what()});
          });

  EitherError<IItem> result = semaphore.get_future().get();

  ASSERT_NE(result.right(), nullptr);

  EXPECT_EQ(result.right()->id(), "238628601250033");
  EXPECT_EQ(result.right()->filename(), "docker_clean.sh");
  EXPECT_EQ(result.right()->size(), 198);
  EXPECT_EQ(result.right()->timestamp(),
            std::chrono::system_clock::from_time_t(1560095432));
  EXPECT_EQ(result.right()->type(), IItem::FileType::Unknown);
}

TEST(MegaNzTest, HandlesGetItemDataFailure) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("mega", {TOKEN});

  EXPECT_CALL(*mock.http(), create)
      .WillRepeatedly(WithArg<0>(Invoke(MockedMegaResponse)));

  std::promise<EitherError<IItem>> semaphore;
  auto request =
      provider->getItemData("1")
          .then([&](const IItem::Pointer& d) { semaphore.set_value(d); })
          .error<IException>([&](const auto& e) {
            semaphore.set_value(Error{e.code(), e.what()});
          });

  EitherError<IItem> result = semaphore.get_future().get();

  ASSERT_NE(result.left(), nullptr);

  EXPECT_EQ(result.left()->code_, IHttpRequest::NotFound);
}

TEST(MegaNzTest, DownloadsItem) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("mega", {TOKEN});

  EXPECT_CALL(*mock.http(), create)
      .WillRepeatedly(WithArg<0>(Invoke(MockedMegaResponse)));

  auto download_callback = std::make_shared<DownloadCallbackMock>();
  EXPECT_CALL(
      *download_callback,
      receivedData(
          StrEq("#!/bin/bash\ndocker kill $(docker ps -q)\ndocker images -a "
                "--filter=dangling=true -q \ndocker rm $(docker ps "
                "--filter=status=exited --filter=status=created -q) -f\ndocker "
                "rmi $(docker images -a -q) -f\n\n*J\xF9\x81\xBB\xD8_K\x3\t"),
          198))
      .Times(1);

  std::promise<EitherError<void>> semaphore;
  provider->getItemData("238628601250033")
      .then([&](const IItem::Pointer& d) {
        return provider->downloadFile(d, FullRange, download_callback);
      })
      .then([&] { semaphore.set_value(nullptr); })
      .error<IException>([&](const auto& e) {
        semaphore.set_value(Error{e.code(), e.what()});
      });

  EitherError<void> result = semaphore.get_future().get();

  EXPECT_EQ(result.left(), nullptr);
}

}  // namespace cloudstorage

#endif
