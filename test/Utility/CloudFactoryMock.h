#ifndef CLOUDSTORAGE_CLOUDFACTORYMOCK_H
#define CLOUDSTORAGE_CLOUDFACTORYMOCK_H

#include "gmock/gmock.h"

#include "HttpMock.h"
#include "HttpServerMock.h"
#include "ICloudFactory.h"
#include "IThreadPool.h"
#include "ThreadPoolMock.h"

namespace cloudstorage {
inline void PrintTo(const Error& error, std::ostream* os) {
  *os << "Error(" << error.code_ << ", " << error.description_ << ")";
}
inline void PrintTo(const IItem& item, std::ostream* os) {
  *os << "Item(id = " << item.id() << ", filename = " << item.filename() << ")";
}
inline void PrintTo(IItem::FileType type, std::ostream* os) {
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
      case IItem::FileType::Unknown:
        return "Unknown";
    }
  }();
  *os << "FileType(" << str << ")";
}
}  // namespace cloudstorage

namespace std::chrono {
inline void PrintTo(std::chrono::system_clock::time_point timestamp,
                    std::ostream* os) {
  *os << "TimeStamp(" << std::chrono::system_clock::to_time_t(timestamp) << ")";
}
}  // namespace std::chrono

class CloudFactoryCallbackMock : public cloudstorage::ICloudFactory::ICallback {
 public:
  MOCK_METHOD(void, onCloudAuthenticationCodeExchangeFailed,
              (const std::string&, const cloudstorage::IException&),
              (override));
  MOCK_METHOD(void, onCloudAuthenticationCodeReceived,
              (const std::string&, const std::string&), (override));
  MOCK_METHOD(void, onCloudCreated,
              (const std::shared_ptr<cloudstorage::ICloudAccess>&), (override));
  MOCK_METHOD(void, onCloudRemoved,
              (const std::shared_ptr<cloudstorage::ICloudAccess>&), (override));
  MOCK_METHOD(void, onEventsAdded, (), (override));
};

class CloudFactoryMock {
 public:
  static CloudFactoryMock create() {
    auto http_mock = std::make_unique<HttpMock>();
    auto http_server_factory_mock = std::make_unique<HttpServerFactoryMock>();
    auto thread_pool_factory_mock = std::make_unique<ThreadPoolFactoryMock>();
    auto callback_mock = std::make_unique<CloudFactoryCallbackMock>();

    auto http_mock_ptr = http_mock.get();
    auto http_server_factory_mock_ptr = http_server_factory_mock.get();
    auto thread_pool_factory_mock_ptr = thread_pool_factory_mock.get();
    auto callback_mock_ptr = callback_mock.get();

    CloudFactoryMock result(
        http_mock_ptr, http_server_factory_mock_ptr,
        thread_pool_factory_mock_ptr, callback_mock_ptr,
        cloudstorage::ICloudFactory::create(
            cloudstorage::ICloudFactory::InitData{
                "http://cloudstorage-test/", std::move(http_mock),
                std::move(http_server_factory_mock), nullptr,
                std::move(thread_pool_factory_mock),
                std::move(callback_mock)}));

    EXPECT_CALL(*result.callback_, onEventsAdded)
        .WillRepeatedly(testing::Invoke(
            [factory = result.factory_.get()] { factory->processEvents(); }));

    auto default_thread_pool = std::make_unique<ThreadPoolMock>();
    ON_CALL(*default_thread_pool, schedule)
        .WillByDefault(testing::InvokeArgument<0>());
    ON_CALL(*result.thread_pool_factory(), create)
        .WillByDefault(
            testing::Return(testing::ByMove(std::move(default_thread_pool))));

    return result;
  }

  HttpMock* http() const { return http_; }
  HttpServerFactoryMock* http_server() const { return http_server_factory_; }
  ThreadPoolFactoryMock* thread_pool_factory() const {
    return thread_pool_factory_mock_;
  }
  CloudFactoryCallbackMock* callback() const { return callback_; }
  cloudstorage::ICloudFactory* factory() const { return factory_.get(); }

 private:
  CloudFactoryMock(HttpMock* http, HttpServerFactoryMock* http_server_factory,
                   ThreadPoolFactoryMock* thread_pool_factory_mock,
                   CloudFactoryCallbackMock* callback,
                   std::unique_ptr<cloudstorage::ICloudFactory> factory)
      : http_(http),
        http_server_factory_(http_server_factory),
        thread_pool_factory_mock_(thread_pool_factory_mock),
        callback_(callback),
        factory_(std::move(factory)) {}

  HttpMock* http_;
  HttpServerFactoryMock* http_server_factory_;
  ThreadPoolFactoryMock* thread_pool_factory_mock_;
  CloudFactoryCallbackMock* callback_;
  std::unique_ptr<cloudstorage::ICloudFactory> factory_;
};

template <typename Result, typename ResultMatcher>
void ExpectImmediatePromise(Promise<Result>&& promise,
                            const ResultMatcher& matcher) {
  EitherError<Result> result;
  promise.then([&](const Result& actual) { result = actual; })
      .template error<cloudstorage::IException>([&](const auto& e) {
        result = Error{e.code(), e.what()};
      });
  EXPECT_EQ(result.left(), nullptr);
  EXPECT_THAT(result.right(), testing::Pointee(matcher));
}

inline void ExpectImmediatePromise(Promise<>&& promise) {
  EitherError<void> result;
  bool promise_invoked = false;
  promise.then([&] { promise_invoked = true; })
      .template error<cloudstorage::IException>([&](const auto& e) {
        result = Error{e.code(), e.what()};
      });
  EXPECT_TRUE(promise_invoked);
  EXPECT_EQ(result.left(), nullptr);
}

template <typename Result, typename ErrorMatcher>
void ExpectFailedPromise(Promise<Result>&& promise,
                         const ErrorMatcher& matcher) {
  EitherError<Result> result;
  promise.then([&](const Result& actual) { result = actual; })
      .template error<cloudstorage::IException>([&](const auto& e) {
        result = Error{e.code(), e.what()};
      });
  EXPECT_EQ(result.right(), nullptr);
  EXPECT_THAT(result.left(), testing::Pointee(matcher));
}

template <typename ErrorMatcher>
void ExpectFailedPromise(Promise<>&& promise, const ErrorMatcher& matcher) {
  EitherError<void> result;
  bool promise_invoked = false;
  promise.then([&] { promise_invoked = true; })
      .template error<cloudstorage::IException>([&](const auto& e) {
        result = Error{e.code(), e.what()};
      });
  EXPECT_FALSE(promise_invoked);
  EXPECT_THAT(result.left(), testing::Pointee(matcher));
}

#endif  // CLOUDSTORAGE_CLOUDFACTORYMOCK_H
