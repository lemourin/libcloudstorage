/*****************************************************************************
 * MockProvider.cpp : MockProvider implementation
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

#include "MockProvider.h"

#include <QFile>

namespace cloudstorage {

template <typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

MockProvider::MockProvider() {}

void MockProvider::initialize(InitData&&) {}

std::string MockProvider::token() const { return ""; }

ICloudProvider::Hints MockProvider::hints() const { return Hints(); }

std::string MockProvider::name() const { return "mock"; }

std::string MockProvider::endpoint() const { return "http://localhost"; }

std::string MockProvider::authorizeLibraryUrl() const { return ""; }

IItem::Pointer MockProvider::rootDirectory() const {
  return std::make_shared<MockItem>("root", IItem::FileType::Directory);
}

ICloudProvider::ListDirectoryRequest::Pointer MockProvider::listDirectoryAsync(
    IItem::Pointer directory, IListDirectoryCallback::Pointer callback) {
  return make_unique<MockListDirectoryRequest>(directory, std::move(callback));
}

ICloudProvider::GetItemRequest::Pointer MockProvider::getItemAsync(
    const std::string& absolute_path, GetItemCallback callback) {
  return make_unique<MockGetItemRequest>(absolute_path, callback);
}

ICloudProvider::DownloadFileRequest::Pointer MockProvider::downloadFileAsync(
    IItem::Pointer item, IDownloadFileCallback::Pointer callback) {
  return make_unique<MockDownloadFileRequest>(item, std::move(callback));
}

ICloudProvider::UploadFileRequest::Pointer MockProvider::uploadFileAsync(
    IItem::Pointer, const std::string&, IUploadFileCallback::Pointer) {
  return nullptr;
}

ICloudProvider::GetItemDataRequest::Pointer MockProvider::getItemDataAsync(
    const std::string& id, GetItemDataCallback callback) {
  return make_unique<MockGetItemDataRequest>(id, callback);
}

ICloudProvider::DeleteItemRequest::Pointer MockProvider::deleteItemAsync(
    IItem::Pointer, DeleteItemCallback callback) {
  callback(false);
  return make_unique<MockDeleteItemRequest>();
}

ICloudProvider::CreateDirectoryRequest::Pointer
MockProvider::createDirectoryAsync(IItem::Pointer parent,
                                   const std::string& name,
                                   CreateDirectoryCallback callback) {
  return make_unique<MockCreateDirectoryRequest>(parent, name, callback);
}

ICloudProvider::MoveItemRequest::Pointer MockProvider::moveItemAsync(
    IItem::Pointer, IItem::Pointer, MoveItemCallback callback) {
  callback(false);
  return make_unique<MockMoveItemRequest>();
}

ICloudProvider::RenameItemRequest::Pointer MockProvider::renameItemAsync(
    IItem::Pointer, const std::string&, RenameItemCallback callback) {
  callback(false);
  return make_unique<MockMoveItemRequest>();
}

ICloudProvider::ListDirectoryRequest::Pointer MockProvider::listDirectoryAsync(
    IItem::Pointer, ListDirectoryCallback) {
  return nullptr;
}

ICloudProvider::DownloadFileRequest::Pointer MockProvider::downloadFileAsync(
    IItem::Pointer, const std::string&, DownloadFileCallback) {
  return nullptr;
}

ICloudProvider::DownloadFileRequest::Pointer MockProvider::getThumbnailAsync(
    IItem::Pointer, const std::string&, DownloadFileCallback) {
  return nullptr;
}

ICloudProvider::UploadFileRequest::Pointer MockProvider::uploadFileAsync(
    IItem::Pointer, const std::string&, UploadFileCallback) {
  return nullptr;
}

ICloudProvider::DownloadFileRequest::Pointer MockProvider::getThumbnailAsync(
    IItem::Pointer item, IDownloadFileCallback::Pointer callback) {
  return make_unique<MockDownloadFileRequest>(item, std::move(callback));
}

MockProvider::MockListDirectoryRequest::MockListDirectoryRequest(
    IItem::Pointer directory, IListDirectoryCallback::Pointer callback)
    : callback_(std::move(callback)), cancelled_(false) {
  result_ = std::async(std::launch::async, [this, directory]() {
    std::vector<IItem::Pointer> result;
    const int file_count = 100;
    for (int i = 0; i < file_count; i++) {
      if (cancelled_) break;
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      if (i % 2 == 0)
        result.push_back(make_unique<MockItem>(
            directory->filename() + "_directory_" + std::to_string(i),
            IItem::FileType::Directory));
      else
        result.push_back(make_unique<MockItem>(
            directory->filename() + "_file_" + std::to_string(i),
            IItem::FileType::Unknown));
      callback_->receivedItem(result.back());
    }
    callback_->done(result);
    return result;
  });
}

MockProvider::MockGetItemRequest::MockGetItemRequest(const std::string& path,
                                                     GetItemCallback callback) {
  if (path.length() % 2 == 0)
    result_ = make_unique<MockItem>(path, IItem::FileType::Directory);
  else
    result_ = make_unique<MockItem>(path, IItem::FileType::Unknown);
  callback(result_);
}

MockProvider::MockGetItemDataRequest::MockGetItemDataRequest(
    const std::string& id, GetItemDataCallback callback) {
  if (id.length() % 2 == 0)
    result_ = make_unique<MockItem>(id, IItem::FileType::Directory);
  else
    result_ = make_unique<MockItem>(id, IItem::FileType::Unknown);
  callback(result_);
}

MockProvider::MockDownloadFileRequest::MockDownloadFileRequest(
    IItem::Pointer item, IDownloadFileCallback::Pointer callback)
    : callback_(std::move(callback)) {
  function_ = std::async(std::launch::async, [this, item]() {
    const int buffer_size = 1024;
    std::vector<char> buffer(buffer_size);
    std::string filename = item->type() == IItem::FileType::Directory
                               ? ":/resources/directory.png"
                               : ":/resources/file.png";
    QFile file(filename.c_str());
    if (!file.open(QFile::ReadOnly)) {
      callback_->error("Not found file: " + filename);
    }
    while (file.bytesAvailable()) {
      callback_->receivedData(buffer.data(),
                              file.read(buffer.data(), buffer_size));
    }
    callback_->done();
  });
}

MockProvider::MockCreateDirectoryRequest::MockCreateDirectoryRequest(
    IItem::Pointer, const std::string&, CreateDirectoryCallback callback) {
  callback(nullptr);
}

}  // namespace cloudstorage
