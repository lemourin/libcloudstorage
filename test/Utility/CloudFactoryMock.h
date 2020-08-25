#ifndef CLOUDSTORAGE_CLOUDFACTORYMOCK_H
#define CLOUDSTORAGE_CLOUDFACTORYMOCK_H

#include "gmock/gmock.h"

#include "HttpMock.h"
#include "HttpServerMock.h"
#include "ICloudFactory.h"

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
    auto callback_mock = std::make_unique<CloudFactoryCallbackMock>();
    struct ThreadPoolFactory : public IThreadPoolFactory {
      IThreadPool::Pointer create(uint32_t size) override {
        return IThreadPool::create(size);
      }
    };

    auto http_mock_ptr = http_mock.get();
    auto http_server_factory_mock_ptr = http_server_factory_mock.get();
    auto callback_mock_ptr = callback_mock.get();

    CloudFactoryMock result(
        http_mock_ptr, http_server_factory_mock_ptr, callback_mock_ptr,
        cloudstorage::ICloudFactory::create(
            cloudstorage::ICloudFactory::InitData{
                "http://cloudstorage-test/", std::move(http_mock),
                std::move(http_server_factory_mock), nullptr,
                std::make_unique<ThreadPoolFactory>(),
                std::move(callback_mock)}));

    EXPECT_CALL(*result.callback_, onEventsAdded)
        .WillRepeatedly(testing::Invoke(
            [factory = result.factory_.get()] { factory->processEvents(); }));

    return result;
  }

  HttpMock* http() const { return http_; }
  HttpServerFactoryMock* http_server() const { return http_server_factory_; }
  CloudFactoryCallbackMock* callback() const { return callback_; }
  cloudstorage::ICloudFactory* factory() const { return factory_.get(); }

 private:
  CloudFactoryMock(HttpMock* http, HttpServerFactoryMock* http_server_factory,
                   CloudFactoryCallbackMock* callback,
                   std::unique_ptr<cloudstorage::ICloudFactory> factory)
      : http_(http),
        http_server_factory_(http_server_factory),
        callback_(callback),
        factory_(std::move(factory)) {}

  HttpMock* http_;
  HttpServerFactoryMock* http_server_factory_;
  CloudFactoryCallbackMock* callback_;
  std::unique_ptr<cloudstorage::ICloudFactory> factory_;
};

#endif  // CLOUDSTORAGE_CLOUDFACTORYMOCK_H
