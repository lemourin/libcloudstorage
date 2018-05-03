/*****************************************************************************
 * IRequest : IRequest interface
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

#ifndef IREQUEST_H
#define IREQUEST_H

#include <functional>
#include <memory>
#include <vector>

#include "IItem.h"

namespace cloudstorage {

struct Error;
struct PageData;

template <class Left, class Right>
class Either;

template <class T>
using EitherError = Either<Error, T>;

struct GeneralData {
  std::string username_;
  uint64_t space_total_;
  uint64_t space_used_;
};

struct PageData {
  IItem::List items_;
  std::string next_token_;  // empty if no next page
};

struct Token {
  std::string token_;
  std::string access_token_;
};

struct Range {
  static constexpr uint64_t Begin = 0;
  static constexpr uint64_t Full = -1;

  uint64_t start_;
  uint64_t size_;
};

const Range FullRange = {Range::Begin, Range::Full};

/**
 * Class representing pending request. When there is no reference to the
 * request, it's immediately cancelled.
 */
class CLOUDSTORAGE_API IGenericRequest {
 public:
  virtual ~IGenericRequest() = default;

  /**
   * Blocks until request finishes.
   */
  virtual void finish() = 0;

  /**
   * Cancels request; may involve curl request cancellation which may last very
   * long if curl was compiled without asynchronous name resolver(c-ares).
   */
  virtual void cancel() = 0;
};

template <class ReturnValue>
class CLOUDSTORAGE_API IRequest : public IGenericRequest {
 public:
  using Pointer = std::unique_ptr<IRequest>;

  /**
   * Retrieves the result, blocks if it wasn't computed just yet.
   * @return result
   */
  virtual ReturnValue result() = 0;
};

template <class... Arguments>
class IGenericCallback {
 public:
  using Pointer = std::shared_ptr<IGenericCallback>;

  virtual ~IGenericCallback() = default;

  virtual void done(Arguments... args) = 0;
};

class IListDirectoryCallback
    : public IGenericCallback<EitherError<IItem::List>> {
 public:
  using Pointer = std::shared_ptr<IListDirectoryCallback>;

  /**
   * Called when directory's child was fetched.
   *
   * @param item fetched item
   */
  virtual void receivedItem(IItem::Pointer item) = 0;
};

class IDownloadFileCallback : public IGenericCallback<EitherError<void>> {
 public:
  using Pointer = std::shared_ptr<IDownloadFileCallback>;

  /**
   * Called when received a part of file.
   *
   * @param data buffer
   * @param length length of buffer
   */
  virtual void receivedData(const char* data, uint32_t length) = 0;

  /**
   * Called when progress has changed.
   *
   * @param total count of bytes to download
   * @param now count of bytes downloaded
   */
  virtual void progress(uint64_t total, uint64_t now) = 0;
};

class IUploadFileCallback : public IGenericCallback<EitherError<IItem>> {
 public:
  using Pointer = std::shared_ptr<IUploadFileCallback>;

  /**
   * Called when upload starts, can be also called when retransmission is
   * required due to network issues.
   */
  virtual void reset() = 0;

  /**
   * Called when the file data should be uploaded.
   *
   * @param data buffer to put data to
   * @param maxlength max count of bytes which can be put to the buffer
   * @return count of bytes put to the buffer
   */
  virtual uint32_t putData(char* data, uint32_t maxlength) = 0;

  /**
   * @return size of currently uploaded file
   */
  virtual uint64_t size() = 0;

  /**
   * Called when upload progress changed.
   *
   * @param total count of bytes to upload
   * @param now count of bytes already uploaded
   */
  virtual void progress(uint64_t total, uint64_t now) = 0;
};

struct Error {
  int code_;
  std::string description_;
};

template <class Left, class Right>
class Either {
 public:
  Either() {}
  Either(const Left& left) : left_(std::make_shared<Left>(left)) {}
  Either(const Right& right) : right_(std::make_shared<Right>(right)) {}
  Either(std::shared_ptr<Left> left) : left_(left) {}
  Either(std::shared_ptr<Right> right) : right_(right) {}

  std::shared_ptr<Left> left() const { return left_; }
  std::shared_ptr<Right> right() const { return right_; }

 private:
  std::shared_ptr<Left> left_;
  std::shared_ptr<Right> right_;
};

template <class Left>
class Either<Left, void> {
 public:
  Either() {}
  Either(std::nullptr_t) {}
  Either(const Left& left) : left_(std::make_shared<Left>(left)) {}
  Either(std::shared_ptr<Left> left) : left_(left) {}

  std::shared_ptr<Left> left() const { return left_; }

 private:
  std::shared_ptr<Left> left_;
};

template <class... Arguments>
class GenericCallback {
 public:
  GenericCallback() {}

  GenericCallback(const GenericCallback& d) : functor_(d.functor_) {}

  template <class Function>
  GenericCallback(const Function& callback)
      : functor_(std::make_shared<Functor>(callback)) {}

  GenericCallback(typename IGenericCallback<Arguments...>::Pointer functor)
      : functor_(functor) {}

  GenericCallback(std::nullptr_t) {}

  operator bool() const { return static_cast<bool>(functor_); }

  void operator()(Arguments... d) const {
    functor_->done(std::forward<Arguments>(d)...);
  }

  template <class Callback, class... CallbackArguments>
  static typename IGenericCallback<Arguments...>::Pointer make(
      CallbackArguments&&... args) {
    return std::make_shared<Callback>(std::forward<CallbackArguments>(args)...);
  }

  void done(Arguments... d) const {
    functor_->done(std::forward<Arguments>(d)...);
  }

 private:
  class Functor : public IGenericCallback<Arguments...> {
   public:
    Functor(const std::function<void(Arguments...)> callback)
        : callback_(callback) {}

    void done(Arguments... args) override { callback_(args...); }

   private:
    std::function<void(Arguments...)> callback_;
  };

  typename IGenericCallback<Arguments...>::Pointer functor_;
};

using ExchangeCodeCallback = GenericCallback<EitherError<Token>>;
using GetItemUrlCallback = GenericCallback<EitherError<std::string>>;
using GetItemCallback = GenericCallback<EitherError<IItem>>;
using GetItemDataCallback = GenericCallback<EitherError<IItem>>;
using DeleteItemCallback = GenericCallback<EitherError<void>>;
using CreateDirectoryCallback = GenericCallback<EitherError<IItem>>;
using MoveItemCallback = GenericCallback<EitherError<IItem>>;
using RenameItemCallback = GenericCallback<EitherError<IItem>>;
using ListDirectoryPageCallback = GenericCallback<EitherError<PageData>>;
using ListDirectoryCallback = GenericCallback<EitherError<IItem::List>>;
using DownloadFileCallback = GenericCallback<EitherError<void>>;
using UploadFileCallback = GenericCallback<EitherError<IItem>>;
using GetThumbnailCallback = GenericCallback<EitherError<void>>;
using GeneralDataCallback = GenericCallback<EitherError<GeneralData>>;

}  // namespace cloudstorage

#endif  // IREQUEST_H
