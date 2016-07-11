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
    std::string filename() const { return filename_; }
    std::string id() const { return filename_; }
    std::string url() const { return ""; }
    bool is_hidden() const { return false; }
    FileType type() const { return type_; }

   private:
    std::string filename_;
    FileType type_;
  };

  class MockListDirectoryRequest : public ListDirectoryRequest {
   public:
    MockListDirectoryRequest(IItem::Pointer directory,
                             IListDirectoryCallback::Pointer callback);
    ~MockListDirectoryRequest() { cancel(); }
    void finish() { result_.get(); }
    void cancel() {
      cancelled_ = true;
      result_.get();
    }
    std::vector<IItem::Pointer> result() { return result_.get(); }

   private:
    IListDirectoryCallback::Pointer callback_;
    std::future<std::vector<IItem::Pointer>> result_;
    std::atomic_bool cancelled_;
  };

  class MockGetItemRequest : public GetItemRequest {
   public:
    MockGetItemRequest(const std::string& path, GetItemCallback);
    void finish() {}
    void cancel() {}
    IItem::Pointer result() { return result_; }

   private:
    IItem::Pointer result_;
  };

  class MockDownloadFileRequest : public DownloadFileRequest {
   public:
    MockDownloadFileRequest(IItem::Pointer item,
                            IDownloadFileCallback::Pointer);
    void finish() { function_.get(); }
    void cancel() { function_.get(); }
    void result() { function_.get(); }

   private:
    IDownloadFileCallback::Pointer callback_;
    std::future<void> function_;
  };

  class MockUploadFileRequest : public UploadFileRequest {
   public:
    void finish() {}
    void cancel() {}
  };

  class MockGetItemDataRequest : public GetItemDataRequest {
   public:
    MockGetItemDataRequest(const std::string& id, GetItemDataCallback);
    void finish() {}
    void cancel() {}
    IItem::Pointer result() { return result_; }

   private:
    IItem::Pointer result_;
  };

  MockProvider();

  void initialize(const std::string& token, ICallback::Pointer,
                  const Hints& hints = Hints());
  std::string token() const;
  Hints hints() const;
  std::string name() const;
  std::string authorizeLibraryUrl() const;
  IItem::Pointer rootDirectory() const;

  ListDirectoryRequest::Pointer listDirectoryAsync(
      IItem::Pointer, IListDirectoryCallback::Pointer);
  GetItemRequest::Pointer getItemAsync(const std::string& absolute_path,
                                       GetItemCallback);
  DownloadFileRequest::Pointer downloadFileAsync(
      IItem::Pointer, IDownloadFileCallback::Pointer);
  UploadFileRequest::Pointer uploadFileAsync(IItem::Pointer,
                                             const std::string& filename,
                                             IUploadFileCallback::Pointer);
  GetItemDataRequest::Pointer getItemDataAsync(const std::string& id,
                                               GetItemDataCallback);
  DownloadFileRequest::Pointer getThumbnailAsync(
      IItem::Pointer item, IDownloadFileCallback::Pointer);
};

}  // namespace cloudstorage

#endif  // MOCKPROVIDER_H
