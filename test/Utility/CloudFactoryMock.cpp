#include "CloudFactoryMock.h"

using ::testing::ByMove;
using ::testing::Invoke;
using ::testing::InvokeArgument;
using ::testing::Return;

namespace cloudstorage {
void PrintTo(const std::shared_ptr<Error>& error, std::ostream* os) {
  if (error) {
    *os << "Error(" << error->code_ << ", " << error->description_ << ")";
  } else {
    *os << "Error(nullptr)";
  }
}
void PrintTo(const IItem& item, std::ostream* os) {
  *os << "Item(id = " << item.id() << ", filename = " << item.filename() << ")";
}
void PrintTo(IItem::FileType type, std::ostream* os) {
  const auto str = [=] {
    switch (type) {
      case IItem::FileType::Image:
        return "Image";
      case IItem::FileType::Audio:
        return "Audio";
      case IItem::FileType::Video:
        return "Video";
      case IItem::FileType::Directory:
        return "Directory";
      default:
        return "Unknown";
    }
  }();
  *os << "FileType(" << str << ")";
}
}  // namespace cloudstorage

namespace std::chrono {
void PrintTo(std::chrono::system_clock::time_point timestamp,
             std::ostream* os) {
  *os << "TimeStamp(" << std::chrono::system_clock::to_time_t(timestamp) << ")";
}
}  // namespace std::chrono

CloudFactoryMock::CloudFactoryMock(
    HttpMock* http, HttpServerFactoryMock* http_server_factory,
    ThreadPoolFactoryMock* thread_pool_factory_mock,
    CloudFactoryCallbackMock* callback,
    std::unique_ptr<cloudstorage::ICloudFactory> factory)
    : http_(http),
      http_server_factory_(http_server_factory),
      thread_pool_factory_mock_(thread_pool_factory_mock),
      callback_(callback),
      factory_(std::move(factory)) {}

CloudFactoryMock CloudFactoryMock::create() {
  auto http_mock = std::make_unique<HttpMock>();
  auto http_server_factory_mock = std::make_unique<HttpServerFactoryMock>();
  auto thread_pool_factory_mock = std::make_unique<ThreadPoolFactoryMock>();
  auto callback_mock = std::make_unique<CloudFactoryCallbackMock>();

  auto http_mock_ptr = http_mock.get();
  auto http_server_factory_mock_ptr = http_server_factory_mock.get();
  auto thread_pool_factory_mock_ptr = thread_pool_factory_mock.get();
  auto callback_mock_ptr = callback_mock.get();

  ON_CALL(*thread_pool_factory_mock, create).WillByDefault([] {
    auto thread_pool = std::make_unique<ThreadPoolMock>();
    ON_CALL(*thread_pool, schedule).WillByDefault(InvokeArgument<0>());
    return thread_pool;
  });

  auto dummy_factory = cloudstorage::ICloudFactory::create(
      std::make_unique<CloudFactoryCallbackMock>());
  std::unordered_map<std::string, cloudstorage::ICloudFactory::ProviderInitData>
      init_data;
  for (const std::string& p : dummy_factory->availableProviders()) {
    cloudstorage::ICloudFactory::ProviderInitData d;
    d.hints_["client_id"] = "client_id";
    d.hints_["client_secret"] = "client_secret";
    d.hints_["redirect_uri"] = "http://redirect-uri/";
    d.hints_["state"] = "state";
    init_data[p] = std::move(d);
  }

  auto cloud_factory =
      cloudstorage::ICloudFactory::create(cloudstorage::ICloudFactory::InitData{
          "http://cloudstorage-test/", std::move(http_mock),
          std::move(http_server_factory_mock), nullptr,
          std::move(thread_pool_factory_mock), std::move(callback_mock),
          std::move(init_data)});

  EXPECT_CALL(*callback_mock_ptr, onEventsAdded)
      .WillRepeatedly(Invoke(
          [factory = cloud_factory.get()] { factory->processEvents(); }));

  return CloudFactoryMock(http_mock_ptr, http_server_factory_mock_ptr,
                          thread_pool_factory_mock_ptr, callback_mock_ptr,
                          std::move(cloud_factory));
}

HttpMock* CloudFactoryMock::http() const { return http_; }

HttpServerFactoryMock* CloudFactoryMock::http_server() const {
  return http_server_factory_;
}

ThreadPoolFactoryMock* CloudFactoryMock::thread_pool_factory() const {
  return thread_pool_factory_mock_;
}

CloudFactoryCallbackMock* CloudFactoryMock::callback() const {
  return callback_;
}

cloudstorage::ICloudFactory* CloudFactoryMock::factory() const {
  return factory_.get();
}

void ExpectImmediatePromise(cloudstorage::Promise<>&& promise) {
  cloudstorage::EitherError<void> result;
  bool promise_invoked = false;
  promise.then([&] { promise_invoked = true; })
      .error<cloudstorage::IException>([&](const auto& e) {
        result = cloudstorage::Error{e.code(), e.what()};
      });
  EXPECT_TRUE(promise_invoked);
  EXPECT_EQ(result.left(), nullptr);
}
