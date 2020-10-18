#include "CloudProviderMock.h"

using ::testing::ByMove;
using ::testing::Return;

HttpMock* CloudProviderMock::http() const {
  return static_cast<HttpMock*>(
      static_cast<const cloudstorage::CloudProvider*>(this)->http());
}

HttpServerFactoryMock* CloudProviderMock::http_server() const {
  return static_cast<HttpServerFactoryMock*>(
      static_cast<const cloudstorage::CloudProvider*>(this)->http_server());
}

AuthCallbackMock* CloudProviderMock::auth_callback() const {
  return static_cast<AuthCallbackMock*>(
      static_cast<const cloudstorage::CloudProvider*>(this)->auth_callback());
}

AuthMock* CloudProviderMock::auth() const {
  return static_cast<AuthMock*>(
      static_cast<const cloudstorage::CloudProvider*>(this)->auth());
}

std::shared_ptr<CloudProviderMock> CloudProviderMock::create() {
  auto auth_mock = std::make_unique<AuthMock>();
  EXPECT_CALL(*auth_mock, login_page).WillRepeatedly(Return("test-login-page"));
  EXPECT_CALL(*auth_mock, success_page)
      .WillRepeatedly(Return("test-success-page"));
  EXPECT_CALL(*auth_mock, error_page).WillRepeatedly(Return("test-error-page"));
  EXPECT_CALL(*auth_mock, state).WillRepeatedly(Return("test-state"));
  EXPECT_CALL(*auth_mock, fromTokenString)
      .WillOnce(Return(ByMove(std::make_unique<cloudstorage::IAuth::Token>())));

  auto auth_callback = std::make_unique<AuthCallbackMock>();
  EXPECT_CALL(*auth_callback, userConsentRequired)
      .WillRepeatedly(Return(IAuthCallback::Status::None));

  auto mock = std::make_shared<CloudProviderMock>(std::move(auth_mock));

  EXPECT_CALL(*mock, name).WillRepeatedly(Return("test-provider"));

  InitData data;
  data.http_engine_ = std::make_unique<HttpMock>();
  data.http_server_ = std::make_unique<HttpServerFactoryMock>();
  data.callback_ = std::move(auth_callback);
  mock->initialize(std::move(data));

  return mock;
}