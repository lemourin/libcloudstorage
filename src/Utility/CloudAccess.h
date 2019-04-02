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
#include "ICloudAccess.h"
#include "ICloudProvider.h"
#include "Promise.h"

namespace cloudstorage {

namespace {
template <class T>
void fulfill(const util::Promise<T>& promise, const EitherError<T>& e) {
  if (e.left()) {
    promise.reject(Exception(e.left()));
  } else {
    promise.fulfill(std::move(*e.right()));
  }
}
template <class T>
void fulfill(const util::Promise<std::shared_ptr<T>>& promise,
             const EitherError<T>& e) {
  if (e.left()) {
    promise.reject(Exception(e.left()));
  } else {
    promise.fulfill(std::move(e.right()));
  }
}

void fulfill(const util::Promise<>& promise, const EitherError<void>& e) {
  if (e.left()) {
    promise.reject(Exception(e.left()));
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

class CloudAccess : public ICloudAccess {
 public:
  using Pointer = std::unique_ptr<CloudAccess>;

  CloudAccess(std::shared_ptr<priv::LoopImpl> loop,
              ICloudProvider::Pointer&& provider);

  ICloudProvider* provider() const { return provider_.get(); }
  std::string name() const override;
  IItem::Pointer root() const override;
  std::string token() const override;

  Promise<Token> exchangeCode(const std::string& code) override;
  Promise<GeneralData> generalData() override;
  Promise<IItem::List> listDirectory(IItem::Pointer item) override;
  Promise<IItem::Pointer> getItem(const std::string& path) override;
  Promise<std::string> getDaemonUrl(IItem::Pointer item) override;
  Promise<std::string> getFileUrl(IItem::Pointer item) override;
  Promise<IItem::Pointer> getItemData(const std::string& id) override;
  Promise<> deleteItem(IItem::Pointer item) override;
  Promise<IItem::Pointer> createDirectory(IItem::Pointer parent,
                                          const std::string& filename) override;
  Promise<IItem::Pointer> moveItem(IItem::Pointer item,
                                   IItem::Pointer new_parent) override;
  Promise<IItem::Pointer> renameItem(IItem::Pointer item,
                                     const std::string& new_name) override;
  Promise<PageData> listDirectoryPage(IItem::Pointer item,
                                      const std::string& token) override;
  Promise<IItem::Pointer> uploadFile(
      IItem::Pointer parent, const std::string& filename,
      std::unique_ptr<ICloudUploadCallback>&&) override;
  Promise<> downloadFile(
      IItem::Pointer file, Range range,
      const std::shared_ptr<ICloudDownloadCallback>&) override;
  Promise<> downloadThumbnail(
      IItem::Pointer file,
      const std::shared_ptr<ICloudDownloadCallback>&) override;
  Promise<> generateThumbnail(
      IItem::Pointer file,
      const std::shared_ptr<ICloudDownloadCallback>&) override;
  Promise<IItem::Pointer> copyItem(
      IItem::Pointer source_item,
      const std::shared_ptr<ICloudAccess>& target_provider,
      IItem::Pointer target_parent, const std::string& target_filename,
      std::unique_ptr<std::iostream>&& buffer,
      const ProgressCallback& progress) override;

 private:
  template <
      typename Method,
      class T = typename RequestType<typename MethodType<Method>::type>::type,
      class Promise = typename std::conditional<
          std::is_void<T>::value, util::Promise<>, util::Promise<T>>::type,
      typename... Args>
  Promise wrap(Method method, Args... args) {
    using RemovedShared = typename RemoveSharedPtr<T>::type;
    Promise promise;
    auto tag = loop_->next_tag();
    auto request = (provider_.get()->*method)(
        args..., [loop = loop_, promise, tag](EitherError<RemovedShared> e) {
          loop->fulfill(tag, [=] { fulfill(promise, e); });
        });
    loop_->add(tag, std::move(request));
    return promise;
  }

  std::shared_ptr<priv::LoopImpl> loop_;
  std::shared_ptr<ICloudProvider> provider_;
};

}  // namespace cloudstorage

#endif  // CLOUDACCESS_H
