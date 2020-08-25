#ifdef WITH_MEGA

#include "gtest/gtest.h"

#include <future>

#include "ICloudAccess.h"
#include "TestData.h"
#include "Utility/CloudFactoryMock.h"

namespace cloudstorage {

using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::Invoke;
using ::testing::InvokeArgument;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;
using ::testing::WithArgs;

const std::string TOKEN(reinterpret_cast<const char*>(mega_token_txt),
                        mega_token_txt_len);

const std::string LOGIN_RESPONSE(
    reinterpret_cast<const char*>(mega_login_response_json),
    mega_login_response_json_len);

const std::string FILES_RESPONSE(
    reinterpret_cast<const char*>(mega_files_response_json),
    mega_files_response_json_len);

auto ImmediatelyRespond(const std::string& string) {
  return DoAll(WithArgs<0, 1, 2, 3>(
      Invoke([=](const IHttpRequest::CompleteCallback& on_completed,
                 const std::shared_ptr<std::istream>& input_stream,
                 const std::shared_ptr<std::ostream>& output_stream,
                 const std::shared_ptr<std::ostream>& error_stream) {
        IHttpRequest::Response response;
        response.http_code_ = 200;
        response.output_stream_ = output_stream;
        response.error_stream_ = error_stream;
        response.headers_.insert(
            {"content-length", std::to_string(string.length())});

        *output_stream << string;

        on_completed(response);
      })));
}

std::shared_ptr<HttpRequestMock> ImmediateRequest(const std::string& string) {
  auto request = std::make_shared<HttpRequestMock>();
  EXPECT_CALL(*request, send).WillRepeatedly(ImmediatelyRespond(string));
  return request;
}

TEST(MegaNzTest, ListsDirectory) {
  auto mock = CloudFactoryMock::create();
  auto provider = mock.factory()->create("mega", {TOKEN});

  EXPECT_CALL(*mock.http(),
              create(HasSubstr("https://g.api.mega.co.nz/cs"), "POST", true))
      .WillOnce(Return(ImmediateRequest(LOGIN_RESPONSE)))
      .WillOnce(Return(ImmediateRequest(FILES_RESPONSE)))
      .WillOnce(Return(ImmediateRequest("[0]")));

  EXPECT_CALL(
      *mock.http(),
      create(
          R"(https://g.api.mega.co.nz/wsc?sn=kT5h5dzLGbI&sid=3Lpm9xrLONJ2W7CwGN9LLEVkbkljbmdRU1ZNi4ck5FBxxE4E_d99A20_tA)",
          "POST", true))
      .WillOnce(Return(ImmediateRequest(
          R"({"w":"https://g.api.mega.co.nz/wsc/_pkb7Bs-cdSjYs6bB9s11Q","sn":"XqlH_atPIrU"})")));

  EXPECT_CALL(
      *mock.http(),
      create(
          R"(https://g.api.mega.co.nz/wsc?c=50&sid=3Lpm9xrLONJ2W7CwGN9LLEVkbkljbmdRU1ZNi4ck5FBxxE4E_d99A20_tA)",
          "POST", true))
      .WillOnce(Return(ImmediateRequest(R"({"c":[],"lsn":"S0eXeaePFAA"})")));

  EXPECT_CALL(
      *mock.http(),
      create(
          R"(https://g.api.mega.co.nz/wsc/_pkb7Bs-cdSjYs6bB9s11Q?sn=XqlH_atPIrU&sid=3Lpm9xrLONJ2W7CwGN9LLEVkbkljbmdRU1ZNi4ck5FBxxE4E_d99A20_tA)",
          "POST", true))
      .WillRepeatedly(Return(std::make_shared<HttpRequestMock>()));

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

}  // namespace cloudstorage

#endif