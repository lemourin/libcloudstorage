#ifndef CLOUDSTORAGE_CLOUDFACTORYMOCK_H
#define CLOUDSTORAGE_CLOUDFACTORYMOCK_H

#include "gmock/gmock.h"

#include "HttpMock.h"
#include "HttpServerMock.h"
#include "ICloudFactory.h"
#include "IThreadPool.h"
#include "ThreadPoolMock.h"

namespace cloudstorage {
void PrintTo(const std::shared_ptr<Error>& error, std::ostream* os);
void PrintTo(const IItem& item, std::ostream* os);
void PrintTo(IItem::FileType type, std::ostream* os);
}  // namespace cloudstorage

namespace std::chrono {
void PrintTo(std::chrono::system_clock::time_point timestamp, std::ostream* os);
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
  static CloudFactoryMock create();

  HttpMock* http() const;
  HttpServerFactoryMock* http_server() const;
  ThreadPoolFactoryMock* thread_pool_factory() const;
  CloudFactoryCallbackMock* callback() const;
  cloudstorage::ICloudFactory* factory() const;

 private:
  CloudFactoryMock(HttpMock* http, HttpServerFactoryMock* http_server_factory,
                   ThreadPoolFactoryMock* thread_pool_factory_mock,
                   CloudFactoryCallbackMock* callback,
                   std::unique_ptr<cloudstorage::ICloudFactory> factory);
  HttpMock* http_;
  HttpServerFactoryMock* http_server_factory_;
  ThreadPoolFactoryMock* thread_pool_factory_mock_;
  CloudFactoryCallbackMock* callback_;
  std::unique_ptr<cloudstorage::ICloudFactory> factory_;
};

template <typename Result, typename ResultMatcher>
void ExpectImmediatePromise(cloudstorage::Promise<Result>&& promise,
                            const ResultMatcher& matcher) {
  cloudstorage::EitherError<Result> result;
  promise.then([&](const Result& actual) { result = actual; })
      .template error<cloudstorage::IException>([&](const auto& e) {
        result = cloudstorage::Error{e.code(), e.what()};
      });
  EXPECT_EQ(result.left(), nullptr);
  EXPECT_THAT(result.right(), testing::Pointee(matcher));
}

void ExpectImmediatePromise(cloudstorage::Promise<>&& promise);

template <typename Result, typename ErrorMatcher>
void ExpectFailedPromise(cloudstorage::Promise<Result>&& promise,
                         const ErrorMatcher& matcher) {
  cloudstorage::EitherError<Result> result;
  promise.then([&](const Result& actual) { result = actual; })
      .template error<cloudstorage::IException>([&](const auto& e) {
        result = cloudstorage::Error{e.code(), e.what()};
      });
  EXPECT_EQ(result.right(), nullptr);
  EXPECT_THAT(result.left(), testing::Pointee(matcher));
}

template <typename ErrorMatcher>
void ExpectFailedPromise(cloudstorage::Promise<>&& promise,
                         const ErrorMatcher& matcher) {
  cloudstorage::EitherError<void> result;
  bool promise_invoked = false;
  promise.then([&] { promise_invoked = true; })
      .template error<cloudstorage::IException>([&](const auto& e) {
        result = cloudstorage::Error{e.code(), e.what()};
      });
  EXPECT_FALSE(promise_invoked);
  EXPECT_THAT(result.left(), testing::Pointee(matcher));
}

#endif  // CLOUDSTORAGE_CLOUDFACTORYMOCK_H
