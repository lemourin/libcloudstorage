#ifndef CLOUDSTORAGE_CLOUDPROVIDERMOCK_H
#define CLOUDSTORAGE_CLOUDPROVIDERMOCK_H

#include "CloudProvider/CloudProvider.h"
#include "gmock/gmock.h"

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
};

#endif  // CLOUDSTORAGE_CLOUDPROVIDERMOCK_H