/*****************************************************************************
 * CloudAccess.h
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
#ifndef CLOUDACCESS_H
#define CLOUDACCESS_H

#include "CloudEventLoop.h"
#include "ICloudProvider.h"
#include "Promise.h"

namespace cloudstorage {

namespace {
template <class T>
void fulfill(const util::Promise<T>& promise, const EitherError<T>& e) {
  if (e.left()) {
    promise.fulfill_exception(std::make_exception_ptr(Exception(e.left())));
  } else {
    promise.fulfill(*e.right());
  }
}
template <class T>
void fulfill(const util::Promise<std::shared_ptr<T>>& promise,
             const EitherError<T>& e) {
  if (e.left()) {
    promise.fulfill_exception(std::make_exception_ptr(Exception(e.left())));
  } else {
    promise.fulfill(e.right());
  }
}

void fulfill(const util::Promise<void>& promise, const EitherError<void>& e) {
  if (e.left()) {
    promise.fulfill_exception(std::make_exception_ptr(Exception(e.left())));
  } else {
    promise.fulfill();
  }
}

template <class T>
struct RemoveSharedPtr {
  using type = T;
};

template <class T>
struct RemoveSharedPtr<std::shared_ptr<T>> {
  using type = T;
};

template <class T>
struct MethodType;

template <class T, class Ret, class... Args>
struct MethodType<Ret (T::*)(Args...)> {
  using type = Ret;
};

template <class T>
struct RequestType;

template <class T>
struct RequestType<std::unique_ptr<IRequest<EitherError<T>>>> {
  using type = T;
};

template <>
struct RequestType<std::unique_ptr<IRequest<EitherError<IItem>>>> {
  using type = std::shared_ptr<IItem>;
};
}  // namespace

class CloudUploadCallback {
 public:
  virtual ~CloudUploadCallback() = default;

  virtual uint32_t putData(char* data, uint32_t maxlength, uint64_t offset) = 0;
  virtual uint64_t size() = 0;
  virtual void progress(uint64_t total, uint64_t now) = 0;
};

class CloudDownloadCallback {
 public:
  virtual ~CloudDownloadCallback() = default;
  virtual void receivedData(const char* data, uint32_t length) = 0;
  virtual void progress(uint64_t total, uint64_t now) = 0;
};

std::unique_ptr<CloudUploadCallback> streamUploader(
    std::unique_ptr<std::istream> stream,
    const std::function<void(uint64_t total, uint64_t now)>& progress =
        nullptr);
std::unique_ptr<CloudDownloadCallback> streamDownloader(
    std::unique_ptr<std::ostream> stream,
    const std::function<void(uint64_t total, uint64_t now)>& progress =
        nullptr);

class CloudAccess {
 public:
  using Pointer = std::unique_ptr<CloudAccess>;

  CloudAccess(std::shared_ptr<priv::LoopImpl> loop,
              ICloudProvider::Pointer&& provider);

  ICloudProvider* provider() const { return provider_.get(); }
  std::string name() const;
  IItem::Pointer root() const;

  util::Promise<Token> exchangeCode(const std::string& code);
  util::Promise<GeneralData> generalData();
  util::Promise<IItem::List> listDirectory(IItem::Pointer item);
  util::Promise<IItem::Pointer> getItem(const std::string& path);
  util::Promise<std::string> getDaemonUrl(IItem::Pointer item);
  util::Promise<std::string> getFileUrl(IItem::Pointer item);
  util::Promise<IItem::Pointer> getItemData(const std::string& id);
  util::Promise<void> deleteItem(IItem::Pointer item);
  util::Promise<IItem::Pointer> createDirectory(IItem::Pointer parent,
                                                const std::string& filename);
  util::Promise<IItem::Pointer> moveItem(IItem::Pointer item,
                                         IItem::Pointer new_parent);
  util::Promise<IItem::Pointer> renameItem(IItem::Pointer item,
                                           const std::string& new_name);
  util::Promise<PageData> listDirectoryPage(IItem::Pointer item,
                                            const std::string& token);
  util::Promise<IItem::Pointer> uploadFile(
      IItem::Pointer parent, const std::string& filename,
      std::unique_ptr<CloudUploadCallback>&&);
  util::Promise<void> downloadFile(IItem::Pointer file, Range range,
                                   std::unique_ptr<CloudDownloadCallback>&&);

 private:
  template <
      typename Method,
      class T = typename RequestType<typename MethodType<Method>::type>::type,
      typename... Args>
  util::Promise<T> wrap(Method method, Args... args) {
    using RemovedShared = typename RemoveSharedPtr<T>::type;
    util::Promise<T> promise;
    auto tag = loop_->next_tag();
    auto request = (provider_.get()->*method)(
        args..., [loop = loop_, promise, tag](EitherError<RemovedShared> e) {
          loop->fulfill(tag, [=] { fulfill(promise, e); });
        });
    loop_->add(tag, std::move(request));
    return promise;
  }

  std::shared_ptr<priv::LoopImpl> loop_;
  ICloudProvider::Pointer provider_;
};

}  // namespace cloudstorage

#endif  // CLOUDACCESS_H
