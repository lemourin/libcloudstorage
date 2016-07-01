/*****************************************************************************
 * Request.h : Request prototypes
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

#ifndef REQUEST_H
#define REQUEST_H

#include <future>
#include <mutex>
#include <sstream>
#include <vector>

#include "IItem.h"

namespace cloudstorage {

class AuthorizeRequest;
class CloudProvider;
class HttpRequest;
namespace {
class HttpCallback;
}  // namespace

class Request {
 public:
  Request(std::shared_ptr<CloudProvider>);
  virtual ~Request() = default;

  virtual void finish() = 0;
  virtual void cancel();

 protected:
  std::unique_ptr<HttpCallback> httpCallback();
  std::stringstream& input_stream() { return input_stream_; }
  std::shared_ptr<CloudProvider> provider() const { return provider_; }
  bool reauthorize();

  void set_cancelled(bool e) { is_cancelled_ = e; }
  bool is_cancelled() { return is_cancelled_; }

 private:
  std::mutex mutex_;
  std::unique_ptr<AuthorizeRequest> authorize_request_;
  std::shared_ptr<CloudProvider> provider_;
  std::stringstream input_stream_;
  std::atomic_bool is_cancelled_;
};

class ListDirectoryRequest : public Request {
 public:
  using Pointer = std::unique_ptr<ListDirectoryRequest>;

  class ICallback {
   public:
    using Pointer = std::unique_ptr<ICallback>;

    virtual ~ICallback() = default;

    virtual void receivedItem(IItem::Pointer item) = 0;
    virtual void done(const std::vector<IItem::Pointer>& result) = 0;
    virtual void error(const std::string& description) = 0;
  };

  ListDirectoryRequest(std::shared_ptr<CloudProvider>, IItem::Pointer directory,
                       ICallback::Pointer);
  ~ListDirectoryRequest();

  void finish();
  std::vector<IItem::Pointer> result();

 private:
  friend class GetItemRequest;

  std::shared_future<std::vector<IItem::Pointer>> result_;
  IItem::Pointer directory_;
  ICallback::Pointer callback_;
};

class GetItemRequest : public Request {
 public:
  using Pointer = std::unique_ptr<GetItemRequest>;

  GetItemRequest(std::shared_ptr<CloudProvider>, const std::string& path,
                 std::function<void(IItem::Pointer)> callback);
  ~GetItemRequest();

  void finish();
  void cancel();
  IItem::Pointer result();

 private:
  IItem::Pointer getItem(const std::vector<IItem::Pointer>& items,
                         const std::string& name) const;

  std::mutex mutex_;
  ListDirectoryRequest::Pointer current_request_;
  std::shared_future<IItem::Pointer> result_;
  std::string path_;
  std::function<void(IItem::Pointer)> callback_;
};

class DownloadFileRequest : public Request {
 public:
  using Pointer = std::unique_ptr<DownloadFileRequest>;

  class ICallback {
   public:
    using Pointer = std::unique_ptr<ICallback>;
    virtual ~ICallback() = default;

    virtual void receivedData(const char* data, uint32_t length) = 0;
    virtual void done() = 0;
    virtual void error(const std::string& description) = 0;
  };

  DownloadFileRequest(std::shared_ptr<CloudProvider>, IItem::Pointer file,
                      ICallback::Pointer);
  ~DownloadFileRequest();

  void finish();

 private:
  class DownloadStreamWrapper : public std::streambuf {
   public:
    DownloadStreamWrapper(DownloadFileRequest::ICallback::Pointer callback);
    std::streamsize xsputn(const char_type* data, std::streamsize length);

    DownloadFileRequest::ICallback::Pointer callback_;
  };

  int download(std::ostream& error_stream);

  std::future<void> function_;
  IItem::Pointer file_;
  DownloadStreamWrapper stream_wrapper_;
};

class UploadFileRequest : public Request {
 public:
  using Pointer = std::unique_ptr<UploadFileRequest>;

  class ICallback {
   public:
    using Pointer = std::unique_ptr<ICallback>;
    virtual ~ICallback() = default;

    virtual void reset() = 0;
    virtual uint32_t putData(char* data, uint32_t maxlength) = 0;
    virtual void done() = 0;
    virtual void error(const std::string& description) = 0;
  };

  UploadFileRequest(std::shared_ptr<CloudProvider>, IItem::Pointer directory,
                    const std::string& filename, ICallback::Pointer);
  ~UploadFileRequest();

  void finish();

 private:
  class UploadStreamWrapper : public std::streambuf {
   public:
    static constexpr uint32_t BUFFER_SIZE = 1024;

    UploadStreamWrapper(UploadFileRequest::ICallback::Pointer callback);

    int_type underflow();

    char buffer_[BUFFER_SIZE];
    UploadFileRequest::ICallback::Pointer callback_;
  };

  int upload(std::ostream& error_stream);

  std::future<void> function_;
  IItem::Pointer directory_;
  std::string filename_;
  UploadStreamWrapper stream_wrapper_;
};

class AuthorizeRequest : public Request {
 public:
  using Pointer = std::unique_ptr<AuthorizeRequest>;

  AuthorizeRequest(std::shared_ptr<CloudProvider>);
  ~AuthorizeRequest();

  bool result();
  void finish();
  void cancel();

 private:
  bool authorize();

  std::mutex mutex_;
  bool awaiting_authorization_code_;
  std::shared_future<bool> function_;
};

}  // namespace cloudstorage

#endif  // REQUEST_H
