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

#ifdef WITH_THUMBNAILER
#include "GenerateThumbnail.h"
#endif

#include <json/json.h>
#include <algorithm>
#include <future>

template <class... T>
using Promise = util::Promise<T...>;

namespace cloudstorage {

const auto MAX_THUMBNAIL_GENERATION_TIME = std::chrono::seconds(10);
const auto CHECK_INTERVAL = std::chrono::milliseconds(100);

namespace {
struct UploadCallback : public IUploadFileCallback {
  UploadCallback(std::unique_ptr<ICloudUploadCallback>&& cb,
                 const Promise<IItem::Pointer>& promise, uint64_t tag,
                 const std::shared_ptr<priv::LoopImpl>& loop)
      : callback_(std::move(cb)), promise_(promise), tag_(tag), loop_(loop) {}

  uint32_t putData(char* data, uint32_t maxlength, uint64_t offset) override {
    return callback_->putData(data, maxlength, offset);
  }

  uint64_t size() override { return callback_->size(); }

  void progress(uint64_t total, uint64_t now) override {
    loop_->invoke(
        [callback = callback_, total, now] { callback->progress(total, now); });
  }

  void done(EitherError<IItem> e) override {
    loop_->fulfill(tag_, [promise = promise_, e] { fulfill(promise, e); });
  }

  std::shared_ptr<ICloudUploadCallback> callback_;
  Promise<IItem::Pointer> promise_;
  uint64_t tag_;
  std::shared_ptr<priv::LoopImpl> loop_;
};

struct DownloadCallback : public IDownloadFileCallback {
  DownloadCallback(const std::shared_ptr<ICloudDownloadCallback>& cb,
                   const Promise<>& promise, uint64_t tag,
                   const std::shared_ptr<priv::LoopImpl>& loop)
      : callback_(cb), promise_(promise), tag_(tag), loop_(loop) {}

  void receivedData(const char* data, uint32_t length) override {
    callback_->receivedData(data, length);
  }

  void progress(uint64_t total, uint64_t now) override {
    loop_->invoke(
        [callback = callback_, total, now] { callback->progress(total, now); });
  }

  void done(EitherError<void> e) override {
    loop_->fulfill(tag_, [promise = promise_, e] { fulfill(promise, e); });
  }

  std::shared_ptr<ICloudDownloadCallback> callback_;
  Promise<> promise_;
  uint64_t tag_;
  std::shared_ptr<priv::LoopImpl> loop_;
};

struct Uploader : public ICloudUploadCallback {
  Uploader(const std::shared_ptr<std::istream>& stream,
           const ICloudAccess::ProgressCallback& progress)
      : stream_(stream), progress_(progress) {}

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

  std::shared_ptr<std::istream> stream_;
  ICloudAccess::ProgressCallback progress_;
};

struct Downloader : public ICloudDownloadCallback {
  Downloader(const std::shared_ptr<std::ostream>& stream,
             const ICloudAccess::ProgressCallback& progress)
      : stream_(stream), progress_(progress) {}

  void receivedData(const char* data, uint32_t length) override {
    stream_->write(data, length);
  }

  void progress(uint64_t total, uint64_t now) override {
    if (progress_) progress_(total, now);
  }

  std::shared_ptr<std::ostream> stream_;
  ICloudAccess::ProgressCallback progress_;
};

bool startsWith(const std::string& string, const std::string& prefix) {
  if (string.length() < prefix.length()) return false;
  return string.substr(0, prefix.length()) == prefix;
}

}  // namespace

void fulfill(const Promise<>& promise, const EitherError<void>& e) {
  if (e.left()) {
    promise.reject(Exception(e.left()));
  } else {
    promise.fulfill();
  }
}

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

Promise<> CloudAccess::deleteItem(IItem::Pointer item) {
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
    std::unique_ptr<ICloudUploadCallback>&& cb) {
  Promise<IItem::Pointer> promise;
  auto tag = loop_->next_tag();
  auto request = provider_->uploadFileAsync(
      parent, filename,
      util::make_unique<UploadCallback>(std::move(cb), promise, tag, loop_));
  loop_->add(tag, std::move(request));
  return promise;
}

Promise<> CloudAccess::downloadFile(
    IItem::Pointer file, Range range,
    const std::shared_ptr<ICloudDownloadCallback>& cb) {
  Promise<> promise;
  auto tag = loop_->next_tag();
  auto request = provider_->downloadFileAsync(
      file, util::make_unique<DownloadCallback>(cb, promise, tag, loop_),
      range);
  loop_->add(tag, std::move(request));
  return promise;
}

Promise<> CloudAccess::downloadThumbnail(
    IItem::Pointer file, const std::shared_ptr<ICloudDownloadCallback>& cb) {
  Promise<> promise;
  auto tag = loop_->next_tag();
  auto request = provider_->getThumbnailAsync(
      file, std::make_shared<DownloadCallback>(cb, promise, tag, loop_));
  loop_->add(tag, std::move(request));
  return promise;
}

Promise<> CloudAccess::generateThumbnail(
    IItem::Pointer item, const std::shared_ptr<ICloudDownloadCallback>& cb) {
  Promise<> result;
  downloadThumbnail(item, cb)
      .then([cb, result] { result.fulfill(); })
      .error<Exception>([item, result, loop = loop_, provider = provider_,
                         cb](const Exception& e) {
#ifdef WITH_THUMBNAILER
        if (item->type() == IItem::FileType::Image ||
            item->type() == IItem::FileType::Video) {
          auto interrupt_atomic = loop->interrupt();
          loop->invokeOnThreadPool([interrupt_atomic, result, item,
                                    provider = provider, loop, cb] {
            uint64_t size = item->size();
            auto interrupt =
                [=](std::chrono::system_clock::time_point start_time) {
                  return *interrupt_atomic ||
                         std::chrono::system_clock::now() - start_time >
                             MAX_THUMBNAIL_GENERATION_TIME;
                };
            if (size == IItem::UnknownSize) {
              std::promise<EitherError<std::string>> url_promise;
              auto url_request = provider->getFileDaemonUrlAsync(
                  item, [&url_promise](EitherError<std::string> e) {
                    url_promise.set_value(e);
                  });
              auto url_future = url_promise.get_future();
              auto start_time = std::chrono::system_clock::now();
              while (url_future.wait_for(CHECK_INTERVAL) !=
                     std::future_status::ready) {
                if (interrupt(start_time)) {
                  url_request->cancel();
                  break;
                }
              }
              auto url = url_future.get();
              if (url.left()) {
                return loop->invoke(
                    [result, url] { result.reject(Exception(url.left())); });
              }
              if (startsWith(*url.right(), "http") &&
                  size == IItem::UnknownSize) {
                auto item_id =
                    url.right()->substr(url.right()->find_last_of('/') + 1);
                std::replace(item_id.begin(), item_id.end(), '/', '-');
                auto json = util::json::from_string(util::from_base64(item_id));
                size = json["size"].asUInt64();
              }
            }
            auto thumb =
                generate_thumbnail(provider.get(), item, size, interrupt);
            if (thumb.left()) {
              return loop->invoke(
                  [result, thumb] { result.reject(Exception(thumb.left())); });
            }
            cb->receivedData(thumb.right()->data(), thumb.right()->size());
            loop->invoke([result] { result.fulfill(); });
          });
        } else {
          result.reject(e);
        }
#else
        result.reject(e);
#endif
      });
  return result;
}

Promise<IItem::Pointer> CloudAccess::copyItem(
    IItem::Pointer source_item,
    const std::shared_ptr<ICloudAccess>& target_provider,
    IItem::Pointer target_parent, const std::string& target_filename,
    std::unique_ptr<std::iostream>&& buffer,
    const ICloudAccess::ProgressCallback& progress) {
  std::shared_ptr<std::iostream> buffer_ptr = std::move(buffer);
  Promise<IItem::Pointer> result;
  if (source_item->type() == IItem::FileType::Directory) {
    result.reject(Exception(600, "Copying directories not implemented"));
    return result;
  }
  if (target_parent->type() != IItem::FileType::Directory) {
    result.reject(Exception(600, "Target not a directory"));
    return result;
  }
  downloadFile(source_item, FullRange,
               streamDownloader(buffer_ptr,
                                [progress](uint64_t total, uint64_t now) {
                                  if (progress) progress(2 * total, now);
                                }))
      .then([target_provider, target_parent, target_filename, progress,
             buffer_ptr] {
        return target_provider->uploadFile(
            target_parent, target_filename,
            streamUploader(buffer_ptr,
                           [progress](uint64_t total, uint64_t now) {
                             if (progress) progress(2 * total, total + now);
                           }));
      })
      .then([result](IItem::Pointer item) { result.fulfill(std::move(item)); })
      .error<Exception>(
          [result](Exception&& e) { result.reject(std::move(e)); });
  return result;
}

std::string CloudAccess::name() const { return provider_->name(); }

IItem::Pointer CloudAccess::root() const { return provider_->rootDirectory(); }

std::string CloudAccess::token() const { return provider_->token(); }

std::unique_ptr<ICloudUploadCallback> ICloudAccess::streamUploader(
    const std::shared_ptr<std::istream>& stream,
    const ICloudAccess::ProgressCallback& progress) {
  return util::make_unique<Uploader>(stream, progress);
}

std::unique_ptr<ICloudDownloadCallback> ICloudAccess::streamDownloader(
    const std::shared_ptr<std::ostream>& stream,
    const ICloudAccess::ProgressCallback& progress) {
  return util::make_unique<Downloader>(stream, progress);
}

}  // namespace cloudstorage
