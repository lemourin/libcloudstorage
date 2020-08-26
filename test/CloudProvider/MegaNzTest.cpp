#ifdef WITH_MEGA

#include "gtest/gtest.h"

#include <future>

#include "ICloudAccess.h"
#include "TestData.h"
#include "Utility/CloudFactoryMock.h"
#include "Utility/Utility.h"

namespace cloudstorage {

using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::Invoke;
using ::testing::Pointee;
using ::testing::Property;
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

}  // namespace cloudstorage

#endif