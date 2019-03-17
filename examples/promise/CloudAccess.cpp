/*****************************************************************************
 * CloudAccess.cpp
 *
 *****************************************************************************
 * Copyright (C) 2019 VideoLAN
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
#include "CloudAccess.h"

#include "Utility/Utility.h"

template <class T>
using Promise = util::Promise<T>;

namespace cloudstorage {

CloudAccess::CloudAccess(std::shared_ptr<priv::LoopImpl> loop,
                         ICloudProvider::Pointer&& provider)
    : loop_(loop), provider_(std::move(provider)) {}

Promise<Token> CloudAccess::exchangeCode(const std::string& code) {
  return wrap(&ICloudProvider::exchangeCodeAsync, code);
}

Promise<GeneralData> CloudAccess::generalData() {
  return wrap(&ICloudProvider::getGeneralDataAsync);
}

Promise<IItem::List> CloudAccess::listDirectory(IItem::Pointer item) {
  return wrap(&ICloudProvider::listDirectorySimpleAsync, item);
}

Promise<IItem::Pointer> CloudAccess::getItem(const std::string& path) {
  return wrap(&ICloudProvider::getItemAsync, path);
}

Promise<std::string> CloudAccess::getDaemonUrl(IItem::Pointer item) {
  return wrap(&ICloudProvider::getFileDaemonUrlAsync, item);
}

Promise<std::string> CloudAccess::getFileUrl(IItem::Pointer item) {
  return wrap(&ICloudProvider::getFileDaemonUrlAsync, item);
}

Promise<IItem::Pointer> CloudAccess::getItemData(const std::string& id) {
  return wrap(&ICloudProvider::getItemDataAsync, id);
}

Promise<void> CloudAccess::deleteItem(IItem::Pointer item) {
  return wrap(&ICloudProvider::deleteItemAsync, item);
}

Promise<IItem::Pointer> CloudAccess::createDirectory(
    IItem::Pointer parent, const std::string& filename) {
  return wrap(&ICloudProvider::createDirectoryAsync, parent, filename);
}

Promise<IItem::Pointer> CloudAccess::moveItem(IItem::Pointer item,
                                              IItem::Pointer new_parent) {
  return wrap(&ICloudProvider::moveItemAsync, item, new_parent);
}

Promise<IItem::Pointer> CloudAccess::renameItem(IItem::Pointer item,
                                                const std::string& new_name) {
  return wrap(&ICloudProvider::renameItemAsync, item, new_name);
}

Promise<PageData> CloudAccess::listDirectoryPage(IItem::Pointer item,
                                                 const std::string& token) {
  return wrap(&ICloudProvider::listDirectoryPageAsync, item, token);
}

Promise<IItem::Pointer> CloudAccess::uploadFile(
    IItem::Pointer parent, const std::string& filename,
    std::unique_ptr<CloudUploadCallback>&& cb) {
  struct UploadCallback : public IUploadFileCallback {
    UploadCallback(std::unique_ptr<CloudUploadCallback>&& cb,
                   const Promise<IItem::Pointer>& promise, uint64_t tag,
                   const std::shared_ptr<priv::LoopImpl>& loop)
        : callback_(std::move(cb)), promise_(promise), tag_(tag), loop_(loop) {}

    uint32_t putData(char* data, uint32_t maxlength, uint64_t offset) override {
      return callback_->putData(data, maxlength, offset);
    }

    uint64_t size() override { return callback_->size(); }

    void progress(uint64_t total, uint64_t now) override {
      callback_->progress(total, now);
    }

    void done(EitherError<IItem> e) override {
      loop_->fulfill(tag_, [promise = promise_, e] { fulfill(promise, e); });
    }

    std::unique_ptr<CloudUploadCallback> callback_;
    Promise<IItem::Pointer> promise_;
    uint64_t tag_;
    std::shared_ptr<priv::LoopImpl> loop_;
  };

  Promise<IItem::Pointer> promise;
  auto tag = loop_->next_tag();
  auto request = provider_->uploadFileAsync(
      parent, filename,
      util::make_unique<UploadCallback>(std::move(cb), promise, tag, loop_));
  loop_->add(tag, std::move(request));
  return promise;
}

Promise<void> CloudAccess::downloadFile(
    IItem::Pointer file, Range range,
    std::unique_ptr<CloudDownloadCallback>&& cb) {
  struct DownloadCallback : public IDownloadFileCallback {
    DownloadCallback(std::unique_ptr<CloudDownloadCallback>&& cb,
                     const Promise<void>& promise, uint64_t tag,
                     const std::shared_ptr<priv::LoopImpl>& loop)
        : callback_(std::move(cb)), promise_(promise), tag_(tag), loop_(loop) {}

    void receivedData(const char* data, uint32_t length) override {
      callback_->receivedData(data, length);
    }

    void progress(uint64_t total, uint64_t now) override {
      callback_->progress(total, now);
    }

    void done(EitherError<void> e) override {
      loop_->fulfill(tag_, [promise = promise_, e] { fulfill(promise, e); });
    }

    std::unique_ptr<CloudDownloadCallback> callback_;
    Promise<void> promise_;
    uint64_t tag_;
    std::shared_ptr<priv::LoopImpl> loop_;
  };

  Promise<void> promise;
  auto tag = loop_->next_tag();
  auto request = provider_->downloadFileAsync(
      file,
      util::make_unique<DownloadCallback>(std::move(cb), promise, tag, loop_),
      range);
  loop_->add(tag, std::move(request));
  return promise;
}

std::string CloudAccess::name() const { return provider_->name(); }

IItem::Pointer CloudAccess::root() const { return provider_->rootDirectory(); }

std::unique_ptr<CloudUploadCallback> streamUploader(
    std::unique_ptr<std::istream> stream,
    const std::function<void(uint64_t total, uint64_t now)>& progress) {
  struct Uploader : public CloudUploadCallback {
    Uploader(std::unique_ptr<std::istream> stream,
             const std::function<void(uint64_t total, uint64_t now)>& progress)
        : stream_(std::move(stream)), progress_(progress) {}

    uint32_t putData(char* data, uint32_t maxlength, uint64_t offset) override {
      stream_->seekg(offset);
      stream_->read(data, maxlength);
      return stream_->gcount();
    }
    uint64_t size() override {
      stream_->seekg(0, std::ios::end);
      auto position = stream_->tellg();
      stream_->seekg(0);
      return position;
    }
    void progress(uint64_t total, uint64_t now) override {
      if (progress_) progress_(total, now);
    }

    std::unique_ptr<std::istream> stream_;
    std::function<void(uint64_t total, uint64_t now)> progress_;
  };
  return util::make_unique<Uploader>(std::move(stream), progress);
}

std::unique_ptr<CloudDownloadCallback> streamDownloader(
    std::unique_ptr<std::ostream> stream,
    const std::function<void(uint64_t, uint64_t)>& progress) {
  struct Downloader : public CloudDownloadCallback {
    Downloader(
        std::unique_ptr<std::ostream> stream,
        const std::function<void(uint64_t total, uint64_t now)>& progress)
        : stream_(std::move(stream)), progress_(progress) {}

    void receivedData(const char* data, uint32_t length) override {
      stream_->write(data, length);
    }

    void progress(uint64_t total, uint64_t now) override {
      if (progress_) progress_(total, now);
    }

    std::unique_ptr<std::ostream> stream_;
    std::function<void(uint64_t total, uint64_t now)> progress_;
  };
  return util::make_unique<Downloader>(std::move(stream), progress);
}

}  // namespace cloudstorage
