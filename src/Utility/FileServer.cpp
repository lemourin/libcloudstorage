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

#include "Utility/Item.h"

namespace cloudstorage {

const int CHUNK_SIZE = 8 * 1024 * 1024;
const int CACHE_SIZE = 128;

namespace {

struct Buffer;
using Cache = util::LRUCache<std::string, IItem>;

class HttpServerCallback : public IHttpServer::ICallback {
 public:
  HttpServerCallback(std::shared_ptr<CloudProvider>);
  IHttpServer::IResponse::Pointer handle(const IHttpServer::IRequest&) override;

 private:
  std::shared_ptr<Cache> item_cache_;
  std::shared_ptr<CloudProvider> provider_;
};

class HttpDataCallback : public IDownloadFileCallback {
 public:
  HttpDataCallback(std::shared_ptr<Buffer> d) : buffer_(d) {}

  void receivedData(const char* data, uint32_t length) override;
  void done(EitherError<void> e) override;
  void progress(uint64_t, uint64_t) override {}

  std::shared_ptr<Buffer> buffer_;
};

struct Buffer : public std::enable_shared_from_this<Buffer> {
  using Pointer = std::shared_ptr<Buffer>;

  int read(char* buf, uint32_t max) {
    if (2 * size() < CHUNK_SIZE) {
      std::unique_lock<std::mutex> lock(delayed_mutex_);
      if (delayed_) {
        delayed_ = false;
        lock.unlock();
        run_download();
      }
    }
    std::unique_lock<std::mutex> lock(mutex_);
    if (abort_) return IHttpServer::IResponse::ICallback::Abort;
    if (data_.empty()) return IHttpServer::IResponse::ICallback::Suspend;
    size_t cnt = std::min<size_t>(data_.size(), static_cast<size_t>(max));
    for (size_t i = 0; i < cnt; i++) {
      buf[i] = data_.front();
      data_.pop();
    }
    return cnt;
  }

  void put(const char* data, uint32_t length) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (uint32_t i = 0; i < length; i++) data_.push(data[i]);
  }

  void done(EitherError<void> e) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (e.left()) {
      abort_ = true;
      if (done_) return;
      done_ = true;
      lock.unlock();
      request_->done(e);
    }
  }

  void resume() {
    std::lock_guard<std::mutex> lock(response_mutex_);
    if (response_) response_->resume();
  }

  std::size_t size() {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_.size();
  }

  void continue_download(EitherError<void> e) {
    if (e.left() || range_.size_ < CHUNK_SIZE) return done(e);
    range_.size_ -= CHUNK_SIZE;
    range_.start_ += CHUNK_SIZE;
    if (2 * size() < CHUNK_SIZE)
      run_download();
    else {
      std::unique_lock<std::mutex> lock(delayed_mutex_);
      delayed_ = true;
    }
  }

  void run_download() {
    request_->make_subrequest(
        &CloudProvider::downloadFileRangeAsync, item_,
        Range{range_.start_, std::min<uint64_t>(range_.size_, CHUNK_SIZE)},
        util::make_unique<HttpDataCallback>(shared_from_this()));
  }

  std::mutex mutex_;
  std::queue<char> data_;
  std::mutex response_mutex_;
  IHttpServer::IResponse* response_;
  Request<EitherError<void>>::Pointer request_;
  IItem::Pointer item_;
  Range range_;
  std::mutex delayed_mutex_;
  bool delayed_ = false;
  bool done_ = false;
  bool abort_ = false;
};

void HttpDataCallback::receivedData(const char* data, uint32_t length) {
  buffer_->put(data, length);
  buffer_->resume();
}

void HttpDataCallback::done(EitherError<void> e) {
  buffer_->resume();
  buffer_->continue_download(e);
}

class HttpData : public IHttpServer::IResponse::ICallback {
 public:
  static constexpr int InProgress = 0;
  static constexpr int Success = 1;
  static constexpr int Failed = 2;

  HttpData(Buffer::Pointer d, std::shared_ptr<CloudProvider> p,
           const std::string& file, Range range, std::shared_ptr<Cache> cache)
      : status_(InProgress),
        buffer_(d),
        provider_(p),
        request_(request(p, file, range, cache)) {}

  ~HttpData() override {
    buffer_->done(Error{IHttpRequest::Aborted, util::Error::ABORTED});
    provider_->removeStreamRequest(request_);
  }

  std::shared_ptr<ICloudProvider::DownloadFileRequest> request(
      std::shared_ptr<CloudProvider> provider, const std::string& file,
      Range range, std::shared_ptr<Cache> cache) {
    auto resolver = [=](Request<EitherError<void>>::Pointer r) {
      auto p = r->provider().get();
      buffer_->request_ = r;
      auto item_received = [=](EitherError<IItem> e) {
        if (e.left()) {
          status_ = Failed;
          buffer_->done(Error{IHttpRequest::Bad, util::Error::INVALID_NODE});
        } else {
          if (range.start_ + range.size_ > uint64_t(e.right()->size())) {
            status_ = Failed;
            buffer_->done(Error{IHttpRequest::Bad, util::Error::INVALID_RANGE});
          } else {
            status_ = Success;
            buffer_->item_ = e.right();
            buffer_->range_ = range;
            cache->put(file, e.right());
            p->addStreamRequest(r);
            r->make_subrequest(
                &CloudProvider::downloadFileRangeAsync, e.right(),
                Range{range.start_,
                      std::min<uint64_t>(range.size_, CHUNK_SIZE)},
                util::make_unique<HttpDataCallback>(buffer_));
          }
        }
        buffer_->resume();
      };
      auto cached_item = cache->get(file);
      if (cached_item == nullptr)
        r->make_subrequest(&CloudProvider::getItemDataAsync, file,
                           item_received);
      else
        item_received(cached_item);
    };
    return std::make_shared<Request<EitherError<void>>>(
               provider,
               [=](EitherError<void> e) {
                 if (e.left()) status_ = Failed;
                 buffer_->resume();
               },
               resolver)
        ->run();
  }

  int putData(char* buf, size_t max) override {
    if (status_ == Failed)
      return Abort;
    else if (status_ == InProgress)
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
    : item_cache_(util::make_unique<Cache>(CACHE_SIZE)), provider_(p) {}

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
      {"Content-Disposition", "inline; filename=\"" + filename + "\""},
      {"Access-Control-Allow-Origin", "*"},
      {"Access-Control-Allow-Headers", "*"}};
  if (request.method() == "OPTIONS")
    return util::response_from_string(request, IHttpRequest::Ok, headers, "");
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
  auto data = util::make_unique<HttpData>(
      buffer, provider_, util::from_base64(id), range, item_cache_);
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
