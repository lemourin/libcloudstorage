/*****************************************************************************
 * Callback.h : Callback prototypes
 *
 *****************************************************************************
 * Copyright (C) 2016 VideoLAN
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

#ifndef CALLBACK_H
#define CALLBACK_H

#include <ICloudStorage.h>
#include <QUrl>
#include <fstream>

class Window;
class ItemModel;

using cloudstorage::IListDirectoryCallback;
using cloudstorage::IDownloadFileCallback;
using cloudstorage::IUploadFileCallback;
using cloudstorage::IItem;

template <typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

class ListDirectoryCallback : public IListDirectoryCallback {
 public:
  ListDirectoryCallback(Window* w);

  void receivedItem(IItem::Pointer item) override;

  void done(const std::vector<IItem::Pointer>&) override;

  void error(const std::string& str) override;

 private:
  Window* window_;
};

class DownloadThumbnailCallback : public IDownloadFileCallback {
 public:
  DownloadThumbnailCallback(ItemModel* i);

  void receivedData(const char* data, uint32_t length) override;

  void done() override;

  void error(const std::string& error) override;

  void progress(uint32_t, uint32_t) override;

 private:
  ItemModel* item_;
  std::string data_;
};

class DownloadFileCallback : public IDownloadFileCallback {
 public:
  DownloadFileCallback(Window*, std::string filename);

  void receivedData(const char* data, uint32_t length) override;
  void done() override;
  void error(const std::string&) override;
  void progress(uint32_t total, uint32_t now) override;

 private:
  Window* window_;
  std::fstream file_;
  std::string filename_;
};

class UploadFileCallback : public IUploadFileCallback {
 public:
  UploadFileCallback(Window*, QUrl url);

  void reset() override;
  uint32_t putData(char* data, uint32_t maxlength) override;
  uint64_t size() override;
  void done() override;
  void error(const std::string& description) override;
  void progress(uint32_t total, uint32_t now) override;

 private:
  Window* window_;
  std::fstream file_;
  uint64_t size_;
};

class CloudProviderCallback : public cloudstorage::ICloudProvider::ICallback {
 public:
  CloudProviderCallback(Window*);

  Status userConsentRequired(const cloudstorage::ICloudProvider&) override;
  void accepted(const cloudstorage::ICloudProvider&) override;
  void declined(const cloudstorage::ICloudProvider&) override;
  void error(const cloudstorage::ICloudProvider&, const std::string&) override;

 private:
  Window* window_;
};

#endif  // CALLBACK_H
