#ifndef CLOUDSTORAGE_CLOUDPROVIDERMOCK_H
#define CLOUDSTORAGE_CLOUDPROVIDERMOCK_H

#include "CloudProvider/CloudProvider.h"
#include "Utility/HttpMock.h"
#include "Utility/HttpServerMock.h"
#include "gmock/gmock.h"

class AuthCallbackMock : public cloudstorage::ICloudProvider::IAuthCallback {
 public:
  MOCK_METHOD(Status, userConsentRequired,
              (const cloudstorage::ICloudProvider&), (override));

  MOCK_METHOD(void, done,
              (const cloudstorage::ICloudProvider&,
               cloudstorage::EitherError<void>),
              (override));
};

class CloudProviderMock : public cloudstorage::CloudProvider {
 public:
  using cloudstorage::CloudProvider::CloudProvider;

  MOCK_METHOD(OperationSet, supportedOperations, (), (const, override));
  MOCK_METHOD(std::string, token, (), (const, override));
  MOCK_METHOD(Hints, hints, (), (const, override));
  MOCK_METHOD(std::string, name, (), (const, override));
  MOCK_METHOD(std::string, endpoint, (), (const, override));
  MOCK_METHOD(std::string, authorizeLibraryUrl, (), (const, override));
  MOCK_METHOD(cloudstorage::IItem::Pointer, rootDirectory, (),
              (const, override));
  MOCK_METHOD(ExchangeCodeRequest::Pointer, exchangeCodeAsync,
              (const std::string&, cloudstorage::ExchangeCodeCallback),
              (override));
  MOCK_METHOD(GetItemUrlRequest::Pointer, getItemUrlAsync,
              (cloudstorage::IItem::Pointer, cloudstorage::GetItemUrlCallback),
              (override));
  MOCK_METHOD(ListDirectoryRequest::Pointer, listDirectoryAsync,
              (cloudstorage::IItem::Pointer,
               cloudstorage::IListDirectoryCallback::Pointer),
              (override));
  MOCK_METHOD(GetItemRequest::Pointer, getItemAsync,
              (const std::string& absolute_path, cloudstorage::GetItemCallback),
              (override));
  MOCK_METHOD(DownloadFileRequest::Pointer, downloadFileAsync,
              (cloudstorage::IItem::Pointer,
               cloudstorage::IDownloadFileCallback::Pointer,
               cloudstorage::Range),
              (override));
  MOCK_METHOD(UploadFileRequest::Pointer, uploadFileAsync,
              (cloudstorage::IItem::Pointer, const std::string&,
               cloudstorage::IUploadFileCallback::Pointer),
              (override));
  MOCK_METHOD(GetItemDataRequest::Pointer, getItemDataAsync,
              (const std::string&, cloudstorage::GetItemDataCallback),
              (override));
  MOCK_METHOD(DownloadFileRequest::Pointer, getThumbnailAsync,
              (cloudstorage::IItem::Pointer item,
               cloudstorage::IDownloadFileCallback::Pointer),
              (override));
  MOCK_METHOD(DeleteItemRequest::Pointer, deleteItemAsync,
              (cloudstorage::IItem::Pointer, cloudstorage::DeleteItemCallback),
              (override));
  MOCK_METHOD(CreateDirectoryRequest::Pointer, createDirectoryAsync,
              (cloudstorage::IItem::Pointer parent, const std::string&,
               cloudstorage::CreateDirectoryCallback));
  MOCK_METHOD(MoveItemRequest::Pointer, moveItemAsync,
              (cloudstorage::IItem::Pointer, cloudstorage::IItem::Pointer,
               cloudstorage::MoveItemCallback),
              (override));
  MOCK_METHOD(RenameItemRequest::Pointer, renameItemAsync,
              (cloudstorage::IItem::Pointer, const std::string&,
               cloudstorage::RenameItemCallback),
              (override));
  MOCK_METHOD(ListDirectoryPageRequest::Pointer, listDirectoryPageAsync,
              (cloudstorage::IItem::Pointer, const std::string&,
               cloudstorage::ListDirectoryPageCallback),
              (override));
  MOCK_METHOD(ListDirectoryRequest::Pointer, listDirectorySimpleAsync,
              (cloudstorage::IItem::Pointer,
               cloudstorage::ListDirectoryCallback),
              (override));
  MOCK_METHOD(DownloadFileRequest::Pointer, downloadFileAsync,
              (cloudstorage::IItem::Pointer, const std::string&,
               cloudstorage::DownloadFileCallback),
              (override));
  MOCK_METHOD(DownloadFileRequest::Pointer, getThumbnailAsync,
              (cloudstorage::IItem::Pointer, const std::string&,
               cloudstorage::GetThumbnailCallback),
              (override));
  MOCK_METHOD(UploadFileRequest::Pointer, uploadFileAsync,
              (cloudstorage::IItem::Pointer, const std::string&,
               const std::string&, cloudstorage::UploadFileCallback),
              (override));
  MOCK_METHOD(GeneralDataRequest::Pointer, getGeneralDataAsync,
              (cloudstorage::GeneralDataCallback), (override));
  MOCK_METHOD(GetItemUrlRequest::Pointer, getFileDaemonUrlAsync,
              (cloudstorage::IItem::Pointer, cloudstorage::GetItemUrlCallback),
              (override));

  HttpMock* http() const {
    return static_cast<HttpMock*>(
        static_cast<const cloudstorage::CloudProvider*>(this)->http());
  }

  HttpServerFactoryMock* http_server() const {
    return static_cast<HttpServerFactoryMock*>(
        static_cast<const cloudstorage::CloudProvider*>(this)->http_server());
  }

  AuthCallbackMock* auth_callback() const {
    return static_cast<AuthCallbackMock*>(
        static_cast<const cloudstorage::CloudProvider*>(this)->auth_callback());
  }

  AuthMock* auth() const {
    return static_cast<AuthMock*>(
        static_cast<const cloudstorage::CloudProvider*>(this)->auth());
  }

  static std::shared_ptr<CloudProviderMock> create() {
    auto auth_mock = std::make_unique<AuthMock>();
    EXPECT_CALL(*auth_mock, login_page)
        .WillRepeatedly(testing::Return("test-login-page"));
    EXPECT_CALL(*auth_mock, success_page)
        .WillRepeatedly(testing::Return("test-success-page"));
    EXPECT_CALL(*auth_mock, error_page)
        .WillRepeatedly(testing::Return("test-error-page"));
    EXPECT_CALL(*auth_mock, state)
        .WillRepeatedly(testing::Return("test-state"));
    EXPECT_CALL(*auth_mock, fromTokenString)
        .WillOnce(
            testing::Return(testing::ByMove(std::make_unique<IAuth::Token>())));

    auto auth_callback = std::make_unique<AuthCallbackMock>();
    EXPECT_CALL(*auth_callback, userConsentRequired)
        .WillRepeatedly(testing::Return(IAuthCallback::Status::None));

    auto mock = std::make_shared<CloudProviderMock>(std::move(auth_mock));

    EXPECT_CALL(*mock, name).WillRepeatedly(testing::Return("test-provider"));

    InitData data;
    data.http_engine_ = std::make_unique<HttpMock>();
    data.http_server_ = std::make_unique<HttpServerFactoryMock>();
    data.callback_ = std::move(auth_callback);
    mock->initialize(std::move(data));

    return mock;
  }
};

#endif  // CLOUDSTORAGE_CLOUDPROVIDERMOCK_H