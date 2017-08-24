/*****************************************************************************
 * MockProvider.h : MockProvider headers
 *
 *****************************************************************************
 * Copyright (C) 2016-2016 VideoLAN
 *
 * Authors: Paweł Wegner <pawel.wegner95@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef MOCKPROVIDER_H
#define MOCKPROVIDER_H

#include <ICloudProvider.h>
#include <future>

namespace cloudstorage {

class MockProvider : public ICloudProvider {
 public:
  class MockItem : public IItem {
   public:
    MockItem(std::string filename, FileType type)
        : filename_(filename), type_(type) {}
    std::string filename() const override { return filename_; }
    std::string extension() const override { return ""; }
    std::string id() const override { return filename_; }
    std::string url() const override { return ""; }
    bool is_hidden() const override { return false; }
    FileType type() const override { return type_; }

   private:
    std::string filename_;
    FileType type_;
  };

  class MockListDirectoryRequest : public ListDirectoryRequest {
   public:
    MockListDirectoryRequest(IItem::Pointer directory,
                             IListDirectoryCallback::Pointer callback);
    ~MockListDirectoryRequest() { cancel(); }
    void finish() override { result_.wait(); }
    void cancel() override {
      cancelled_ = true;
      result_.wait();
    }
    EitherError<std::vector<IItem::Pointer>> result() override {
      return result_.get();
    }

   private:
    IListDirectoryCallback::Pointer callback_;
    std::future<std::vector<IItem::Pointer>> result_;
    std::atomic_bool cancelled_;
  };

  class MockGetItemRequest : public GetItemRequest {
   public:
    MockGetItemRequest(const std::string& path, GetItemCallback);
    void finish() override {}
    void cancel() override {}
    EitherError<IItem> result() override { return result_; }

   private:
    IItem::Pointer result_;
  };

  class MockDownloadFileRequest : public DownloadFileRequest {
   public:
    MockDownloadFileRequest(IItem::Pointer item,
                            IDownloadFileCallback::Pointer);
    void finish() override { function_.wait(); }
    void cancel() override { function_.wait(); }
    EitherError<void> result() override {
      function_.get();
      return nullptr;
    }

   private:
    IDownloadFileCallback::Pointer callback_;
    std::future<void> function_;
  };

  class MockUploadFileRequest : public UploadFileRequest {
   public:
    void finish() override {}
    void cancel() override {}
  };

  class MockGetItemDataRequest : public GetItemDataRequest {
   public:
    MockGetItemDataRequest(const std::string& id, GetItemDataCallback);
    void finish() override {}
    void cancel() override {}
    EitherError<IItem> result() override { return result_; }

   private:
    IItem::Pointer result_;
  };

  class MockDeleteItemRequest : public DeleteItemRequest {
   public:
    void finish() override {}
    void cancel() override {}
    EitherError<void> result() override { return Error{500, ""}; }
  };

  class MockCreateDirectoryRequest : public CreateDirectoryRequest {
   public:
    MockCreateDirectoryRequest(IItem::Pointer, const std::string& name,
                               CreateDirectoryCallback);
    void finish() override {}
    void cancel() override {}
    EitherError<IItem> result() override { return Error{500, "unimplemented"}; }
  };

  class MockMoveItemRequest : public MoveItemRequest {
   public:
    void finish() override {}
    void cancel() override {}
    EitherError<void> result() override { return Error{500, "unimplemented"}; }
  };

  MockProvider();

  std::string token() const override;
  Hints hints() const override;
  std::string name() const override;
  std::string endpoint() const override;
  std::string authorizeLibraryUrl() const override;
  IItem::Pointer rootDirectory() const override;

  ExchangeCodeRequest::Pointer exchangeCodeAsync(const std::string&,
                                                 ExchangeCodeCallback) override;
  ListDirectoryRequest::Pointer listDirectoryAsync(
      IItem::Pointer, IListDirectoryCallback::Pointer) override;
  GetItemRequest::Pointer getItemAsync(const std::string& absolute_path,
                                       GetItemCallback) override;
  DownloadFileRequest::Pointer downloadFileAsync(
      IItem::Pointer, IDownloadFileCallback::Pointer) override;
  UploadFileRequest::Pointer uploadFileAsync(
      IItem::Pointer, const std::string& filename,
      IUploadFileCallback::Pointer) override;
  GetItemDataRequest::Pointer getItemDataAsync(const std::string& id,
                                               GetItemDataCallback) override;
  DownloadFileRequest::Pointer getThumbnailAsync(
      IItem::Pointer item, IDownloadFileCallback::Pointer) override;
  DeleteItemRequest::Pointer deleteItemAsync(IItem::Pointer,
                                             DeleteItemCallback) override;
  CreateDirectoryRequest::Pointer createDirectoryAsync(
      IItem::Pointer, const std::string&, CreateDirectoryCallback) override;
  MoveItemRequest::Pointer moveItemAsync(IItem::Pointer, IItem::Pointer,
                                         MoveItemCallback) override;
  RenameItemRequest::Pointer renameItemAsync(IItem::Pointer, const std::string&,
                                             RenameItemCallback) override;
  ListDirectoryRequest::Pointer listDirectoryAsync(
      IItem::Pointer, ListDirectoryCallback) override;
  ListDirectoryPageRequest::Pointer listDirectoryPageAsync(
      IItem::Pointer, const std::string&, ListDirectoryPageCallback) override;
  DownloadFileRequest::Pointer downloadFileAsync(IItem::Pointer,
                                                 const std::string&,
                                                 DownloadFileCallback) override;
  DownloadFileRequest::Pointer getThumbnailAsync(IItem::Pointer,
                                                 const std::string&,
                                                 DownloadFileCallback) override;
  UploadFileRequest::Pointer uploadFileAsync(IItem::Pointer, const std::string&,
                                             const std::string&,
                                             UploadFileCallback) override;
};

}  // namespace cloudstorage

#endif  // MOCKPROVIDER_H
