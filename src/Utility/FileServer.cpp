/*****************************************************************************
 * FileServer.cpp
 *
 *****************************************************************************
 * Copyright (C) 2018 VideoLAN
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
#include "FileServer.h"

#include <queue>

namespace cloudstorage {

const int MAX_BUFFER_SIZE = 8 * 1024 * 1024;

namespace {

class HttpServerCallback : public IHttpServer::ICallback {
 public:
  HttpServerCallback(std::shared_ptr<CloudProvider>);
  IHttpServer::IResponse::Pointer handle(const IHttpServer::IRequest&) override;

 private:
  std::shared_ptr<CloudProvider> provider_;
};

struct Buffer {
  using Pointer = std::shared_ptr<Buffer>;

  int read(char* buf, uint32_t max) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (done_) return IHttpServer::IResponse::ICallback::Abort;
    if (data_.empty()) return IHttpServer::IResponse::ICallback::Suspend;
    size_t cnt = std::min<size_t>(data_.size(), static_cast<size_t>(max));
    for (size_t i = 0; i < cnt; i++) {
      buf[i] = data_.front();
      data_.pop();
    }
    return cnt;
  }

  void put(const char* data, uint32_t length) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      for (uint32_t i = 0; i < length; i++) data_.push(data[i]);
    }
    if (2 * size() < MAX_BUFFER_SIZE) {
      request_->resume();
    }
  }

  void done() {
    std::lock_guard<std::mutex> lock(mutex_);
    done_ = true;
  }

  void resume() {
    std::lock_guard<std::mutex> lock(response_mutex_);
    if (response_) response_->resume();
  }

  std::size_t size() {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_.size();
  }

  std::mutex mutex_;
  std::queue<char> data_;
  std::mutex response_mutex_;
  IHttpServer::IResponse* response_;
  Request<EitherError<void>>::Pointer request_;
  bool done_ = false;
};

class HttpDataCallback : public IDownloadFileCallback {
 public:
  HttpDataCallback(Buffer::Pointer d) : buffer_(d) {}

  void receivedData(const char* data, uint32_t length) override {
    buffer_->put(data, length);
    buffer_->resume();
    if (buffer_->size() >= MAX_BUFFER_SIZE) buffer_->request_->pause();
  }

  void done(EitherError<void> e) override {
    buffer_->resume();
    buffer_->request_->done(e);
  }

  void progress(uint64_t, uint64_t) override {}

  Buffer::Pointer buffer_;
};

class HttpData : public IHttpServer::IResponse::ICallback {
 public:
  static constexpr int AuthInProgress = 0;
  static constexpr int AuthSuccess = 1;
  static constexpr int AuthFailed = 2;

  HttpData(Buffer::Pointer d, std::shared_ptr<CloudProvider> p,
           const std::string& file, Range range)
      : status_(AuthInProgress),
        buffer_(d),
        provider_(p),
        request_(request(p, file, range)) {}

  ~HttpData() override {
    buffer_->done();
    provider_->removeStreamRequest(request_);
  }

  std::shared_ptr<ICloudProvider::DownloadFileRequest> request(
      std::shared_ptr<CloudProvider> provider, const std::string& file,
      Range range) {
    auto resolver = [=](Request<EitherError<void>>::Pointer r) {
      auto p = r->provider().get();
      buffer_->request_ = r;
      r->make_subrequest(
          &CloudProvider::getItemDataAsync, file, [=](EitherError<IItem> e) {
            if (e.left()) {
              status_ = AuthFailed;
              r->done(Error{IHttpRequest::Bad, util::Error::INVALID_NODE});
            } else {
              if (range.start_ + range.size_ > uint64_t(e.right()->size())) {
                status_ = AuthFailed;
                r->done(Error{IHttpRequest::Bad, util::Error::INVALID_RANGE});
              } else {
                status_ = AuthSuccess;
                p->addStreamRequest(r);
                r->make_subrequest(
                    &CloudProvider::downloadFileRangeAsync, e.right(), range,
                    util::make_unique<HttpDataCallback>(buffer_));
              }
            }
            buffer_->resume();
          });
    };
    return std::make_shared<Request<EitherError<void>>>(
               provider,
               [=](EitherError<void> e) {
                 if (e.left()) status_ = AuthFailed;
                 buffer_->resume();
               },
               resolver)
        ->run();
  }

  int putData(char* buf, size_t max) override {
    if (status_ == AuthFailed)
      return Abort;
    else if (status_ == AuthInProgress)
      return Suspend;
    else
      return buffer_->read(buf, max);
  }

  std::atomic_int status_;
  Buffer::Pointer buffer_;
  std::shared_ptr<CloudProvider> provider_;
  std::shared_ptr<ICloudProvider::DownloadFileRequest> request_;
};

HttpServerCallback::HttpServerCallback(std::shared_ptr<CloudProvider> p)
    : provider_(p) {}

IHttpServer::IResponse::Pointer HttpServerCallback::handle(
    const IHttpServer::IRequest& request) {
  const char* state = request.get("state");
  const char* name = request.get("name");
  const char* id = request.get("id");
  const char* size_parameter = request.get("size");
  if (!state || state != provider_->auth()->state() || !name || !id ||
      !size_parameter)
    return util::response_from_string(request, IHttpRequest::Bad, {},
                                      util::Error::INVALID_REQUEST);
  std::string filename = util::from_base64(name);
  auto size = std::stoull(size_parameter);
  auto extension = filename.substr(filename.find_last_of('.') + 1);
  std::unordered_map<std::string, std::string> headers = {
      {"Content-Type", util::to_mime_type(extension)},
      {"Accept-Ranges", "bytes"},
      {"Content-Disposition", "inline; filename=\"" + filename + "\""}};
  Range range = {0, size};
  int code = IHttpRequest::Ok;
  if (const char* range_str = request.header("Range")) {
    range = util::parse_range(range_str);
    if (range.size_ == Range::Full) range.size_ = size - range.start_;
    if (range.start_ + range.size_ > size)
      return util::response_from_string(request, IHttpRequest::RangeInvalid, {},
                                        util::Error::INVALID_RANGE);
    std::stringstream stream;
    stream << "bytes " << range.start_ << "-" << range.start_ + range.size_ - 1
           << "/" << size;
    headers["Content-Range"] = stream.str();
    code = IHttpRequest::Partial;
  }
  auto buffer = std::make_shared<Buffer>();
  auto data = util::make_unique<HttpData>(buffer, provider_,
                                          util::from_base64(id), range);
  auto response = request.response(code, headers, range.size_, std::move(data));
  buffer->response_ = response.get();
  response->completed([buffer]() {
    std::unique_lock<std::mutex> lock(buffer->response_mutex_);
    buffer->response_ = nullptr;
  });
  return response;
}

}  // namespace

IHttpServer::Pointer FileServer::create(std::shared_ptr<CloudProvider> p,
                                        const std::string& session) {
  return p->http_server()->create(util::make_unique<HttpServerCallback>(p),
                                  session, IHttpServer::Type::FileProvider);
}

FileServer::FileServer() {}

}  // namespace cloudstorage
