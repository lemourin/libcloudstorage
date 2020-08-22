#ifndef CLOUDSTORAGE_AUTHMOCK_H
#define CLOUDSTORAGE_AUTHMOCK_H

#include "IAuth.h"
#include "gmock/gmock.h"

class AuthMock : public cloudstorage::IAuth {
 public:
  MOCK_METHOD(void, initialize,
              (cloudstorage::IHttp*, cloudstorage::IHttpServerFactory*),
              (override));
  MOCK_METHOD(cloudstorage::IHttp*, http, (), (const, override));

  MOCK_METHOD(const std::string&, authorization_code, (), (const, override));
  MOCK_METHOD(void, set_authorization_code, (const std::string&), (override));

  MOCK_METHOD(const std::string&, client_id, (), (const, override));
  MOCK_METHOD(void, set_client_id, (const std::string&), (override));

  MOCK_METHOD(const std::string&, client_secret, (), (const, override));
  MOCK_METHOD(void, set_client_secret, (const std::string&), (override));

  MOCK_METHOD(std::string, redirect_uri, (), (const, override));
  MOCK_METHOD(void, set_redirect_uri, (const std::string&), (override));

  MOCK_METHOD(cloudstorage::ICloudProvider::Permission, permission, (),
              (const, override));
  MOCK_METHOD(void, set_permission, (cloudstorage::ICloudProvider::Permission),
              (override));

  MOCK_METHOD(std::string, state, (), (const, override));
  MOCK_METHOD(void, set_state, (const std::string&), (override));

  MOCK_METHOD(std::string, login_page, (), (const, override));
  MOCK_METHOD(void, set_login_page, (const std::string&), (override));

  MOCK_METHOD(std::string, success_page, (), (const, override));
  MOCK_METHOD(void, set_success_page, (const std::string&), (override));

  MOCK_METHOD(std::string, error_page, (), (const, override));
  MOCK_METHOD(void, set_error_page, (const std::string&), (override));

  MOCK_METHOD(Token*, access_token, (), (const, override));
  MOCK_METHOD(void, set_access_token, (Token::Pointer), (override));

  MOCK_METHOD(std::string, authorizeLibraryUrl, (), (const, override));

  MOCK_METHOD(cloudstorage::IHttpServer::Pointer, awaitAuthorizationCode,
              (std::string, std::string, std::string, CodeReceived),
              (const, override));

  MOCK_METHOD(cloudstorage::IHttpServer::Pointer, requestAuthorizationCode,
              (CodeReceived), (const, override));

  MOCK_METHOD(Token::Pointer, fromTokenString, (const std::string&),
              (const, override));

  MOCK_METHOD(cloudstorage::IHttpRequest::Pointer,
              exchangeAuthorizationCodeRequest, (std::ostream&),
              (const, override));

  MOCK_METHOD(cloudstorage::IHttpRequest::Pointer, refreshTokenRequest,
              (std::ostream&), (const, override));

  MOCK_METHOD(Token::Pointer, exchangeAuthorizationCodeResponse,
              (std::istream&), (const, override));

  MOCK_METHOD(Token::Pointer, refreshTokenResponse, (std::istream&),
              (const, override));
};

#endif  // CLOUDSTORAGE_AUTHMOCK_H
