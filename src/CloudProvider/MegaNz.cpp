/*****************************************************************************
 * MegaNz.cpp : Mega implementation
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

#include "MegaNz.h"

#include "IAuth.h"
#include "Request/DownloadFileRequest.h"
#include "Request/Request.h"
#include "Utility/Item.h"
#include "Utility/Utility.h"

#include <array>
#include <condition_variable>
#include <cstring>
#include <fstream>
#include <queue>

using namespace mega;
using namespace std::placeholders;

const int BUFFER_SIZE = 1024;
const int CACHE_FILENAME_LENGTH = 12;
const std::string DEFAULT_FILE_URL = "http://localhost:12346";

namespace cloudstorage {

namespace {

class Listener : public IRequest<EitherError<void>>,
                 public std::enable_shared_from_this<Listener> {
 public:
  using Callback = std::function<void(EitherError<void>, Listener*)>;

  static constexpr int IN_PROGRESS = -1;
  static constexpr int FAILURE = 0;
  static constexpr int SUCCESS = 1;
  static constexpr int CANCELLED = 2;

  Listener(Callback cb, MegaNz* provider)
      : status_(IN_PROGRESS),
        error_({IHttpRequest::Unknown, ""}),
        callback_(cb),
        provider_(provider) {}

  ~Listener() { cancel(); }

  void cancel() override {
    std::unique_lock<std::mutex> lock(mutex_);
    if (status_ != IN_PROGRESS) return;
    status_ = CANCELLED;
    error_ = {IHttpRequest::Aborted, ""};
    auto callback = std::move(callback_);
    lock.unlock();
    if (callback) callback(error_, this);
    finish();
  }

  EitherError<void> result() override {
    finish();
    std::lock_guard<std::mutex> lock(mutex_);
    if (status_ != SUCCESS)
      return error_;
    else
      return nullptr;
  }

  void finish() override {
    std::unique_lock<std::mutex> lock(mutex_);
    condition_.wait(lock, [this]() { return status_ != IN_PROGRESS; });
  }

  int status() {
    std::lock_guard<std::mutex> lock(mutex_);
    return status_;
  }

  Callback callback() const { return callback_; }

 protected:
  int status_;
  std::mutex mutex_;
  std::condition_variable condition_;
  Error error_;
  Callback callback_;
  MegaNz* provider_;
};

class RequestListener : public mega::MegaRequestListener, public Listener {
 public:
  using Listener::Listener;

  void onRequestStart(MegaApi*, MegaRequest*) override {
    provider_->addRequestListener(shared_from_this());
  }

  void onRequestFinish(MegaApi*, MegaRequest* r, MegaError* e) override {
    auto p = shared_from_this();
    provider_->removeRequestListener(p);
    std::unique_lock<std::mutex> lock(mutex_);
    if (e->getErrorCode() == 0)
      status_ = SUCCESS;
    else {
      status_ = FAILURE;
      error_ = {e->getErrorCode(), e->getErrorString()};
    }
    if (r->getLink()) link_ = r->getLink();
    node_ = r->getNodeHandle();
    auto callback = std::move(callback_);
    lock.unlock();
    if (callback) {
      if (e->getErrorCode() == 0)
        callback(nullptr, this);
      else
        callback(error_, this);
    }
    condition_.notify_all();
  }

  std::string link_;
  MegaHandle node_;
};

class TransferListener : public mega::MegaTransferListener, public Listener {
 public:
  using Listener::Listener;

  void cancel() override {
    std::unique_lock<std::mutex> lock(mutex_);
    auto mega = std::move(mega_);
    auto transfer = std::move(transfer_);
    upload_callback_ = nullptr;
    download_callback_ = nullptr;
    lock.unlock();
    if (mega) {
      std::unique_ptr<MegaTransfer> t(mega->getTransferByTag(transfer));
      if (t) mega->cancelTransfer(t.get());
    }
    Listener::cancel();
  }

  void onTransferStart(MegaApi* mega, MegaTransfer* transfer) {
    provider_->addRequestListener(shared_from_this());
    std::lock_guard<std::mutex> lock(mutex_);
    if (status_ == CANCELLED) {
      mega->cancelTransfer(transfer);
    } else {
      mega_ = mega;
      transfer_ = transfer->getTag();
    }
  }

  bool onTransferData(MegaApi*, MegaTransfer* t, char* buffer,
                      size_t size) override {
    if (status() == CANCELLED) return false;
    std::unique_lock<std::mutex> lock(mutex_);
    if (download_callback_) {
      download_callback_->receivedData(buffer, size);
      download_callback_->progress(t->getTotalBytes(),
                                   t->getTransferredBytes());
    }
    return true;
  }

  void onTransferUpdate(MegaApi* mega, MegaTransfer* t) override {
    if (status() == CANCELLED) return mega->cancelTransfer(t);
    std::unique_lock<std::mutex> lock(mutex_);
    if (upload_callback_)
      upload_callback_->progress(t->getTotalBytes(), t->getTransferredBytes());
  }

  void onTransferFinish(MegaApi*, MegaTransfer*, MegaError* e) override {
    auto r = shared_from_this();
    provider_->removeRequestListener(r);
    std::unique_lock<std::mutex> lock(mutex_);
    if (e->getErrorCode() == 0)
      status_ = SUCCESS;
    else {
      error_ = {e->getErrorCode(), e->getErrorString()};
      status_ = FAILURE;
    }
    auto callback = std::move(callback_);
    lock.unlock();
    if (callback) {
      if (e->getErrorCode() == 0)
        callback(nullptr, this);
      else
        callback(error_, this);
    }
    condition_.notify_all();
  }

  IDownloadFileCallback::Pointer download_callback_;
  IUploadFileCallback::Pointer upload_callback_;
  MegaApi* mega_ = nullptr;
  int transfer_ = 0;
};

struct Buffer {
  using Pointer = std::shared_ptr<Buffer>;

  int read(char* buf, uint32_t max) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (done_) return IHttpServer::IResponse::ICallback::Abort;
    if (data_.empty()) return IHttpServer::IResponse::ICallback::Suspend;
    size_t cnt = std::min(data_.size(), (size_t)max);
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

  void done() {
    std::lock_guard<std::mutex> lock(mutex_);
    done_ = true;
  }

  void resume() {
    std::lock_guard<std::mutex> lock(response_mutex_);
    if (response_) response_->resume();
  }

  std::mutex mutex_;
  std::queue<char> data_;
  std::mutex response_mutex_;
  IHttpServer::IResponse* response_;
  bool done_ = false;
};

class HttpData : public IHttpServer::IResponse::ICallback {
 public:
  HttpData(Buffer::Pointer d, MegaNz* p,
           std::shared_ptr<Request<EitherError<void>>> r)
      : buffer_(d), mega_(p), request_(r) {}

  ~HttpData() { mega_->removeStreamRequest(request_); }

  int putData(char* buf, size_t max) override {
    return buffer_->read(buf, max);
  }

  Buffer::Pointer buffer_;
  MegaNz* mega_;
  std::shared_ptr<Request<EitherError<void>>> request_;
};

class HttpDataCallback : public IDownloadFileCallback {
 public:
  HttpDataCallback(Buffer::Pointer d) : buffer_(d) {}

  void receivedData(const char* data, uint32_t length) override {
    buffer_->put(data, length);
    buffer_->resume();
  }

  void done(EitherError<void>) override {
    buffer_->done();
    buffer_->resume();
  }

  void progress(uint32_t, uint32_t) override {}

  Buffer::Pointer buffer_;
};

}  // namespace

MegaNz::HttpServerCallback::HttpServerCallback(MegaNz* p) : provider_(p) {}

IHttpServer::IResponse::Pointer MegaNz::HttpServerCallback::handle(
    const IHttpServer::IRequest& request) {
  {
    std::lock_guard<std::mutex> lock(provider_->mutex_);
    if (provider_->deleted_)
      return util::response_from_string(request, IHttpRequest::Bad, {}, "");
  }
  const char* state = request.get("state");
  if (!state || state != provider_->auth()->state())
    return util::response_from_string(request, IHttpRequest::Forbidden, {},
                                      "state parameter missing / invalid");
  if (!provider_->authorized_)
    return util::response_from_string(request, IHttpRequest::ServiceUnavailable,
                                      {}, "not authorized just yet");
  const char* file = request.get("file");
  std::unique_ptr<mega::MegaNode> node(provider_->mega()->getNodeByPath(file));
  if (!node)
    return util::response_from_string(request, IHttpRequest::NotFound, {},
                                      "file not found");
  int code = IHttpRequest::Ok;
  auto extension =
      static_cast<Item*>(provider_->toItem(node.get()).get())->extension();
  std::unordered_map<std::string, std::string> headers = {
      {"Content-Type", util::to_mime_type(extension)},
      {"Accept-Ranges", "bytes"},
      {"Content-Disposition",
       "inline; filename=\"" + std::string(node->getName()) + "\""}};
  util::range range = {0, node->getSize()};
  if (const char* range_str = request.header("Range")) {
    range = util::parse_range(range_str);
    if (range.size == -1) range.size = node->getSize() - range.start;
    if (range.start + range.size > node->getSize() || range.start == -1 ||
        range.size < 0)
      return util::response_from_string(request, IHttpRequest::RangeInvalid, {},
                                        "invalid range");
    std::stringstream stream;
    stream << "bytes " << range.start << "-" << range.start + range.size - 1
           << "/" << node->getSize();
    headers["Content-Range"] = stream.str();
    code = IHttpRequest::Partial;
  }
  auto buffer = std::make_shared<Buffer>();
  auto download_request = std::make_shared<Request<EitherError<void>>>(
      std::weak_ptr<CloudProvider>(provider_->shared_from_this()));
  download_request->set(provider_->downloadResolver(
      provider_->toItem(node.get()),
      util::make_unique<HttpDataCallback>(buffer), range.start, range.size));
  provider_->addStreamRequest(download_request);
  auto data = util::make_unique<HttpData>(buffer, provider_, download_request);
  auto response = request.response(code, headers, range.size, std::move(data));
  buffer->response_ = response.get();
  response->completed([buffer]() {
    std::unique_lock<std::mutex> lock(buffer->response_mutex_);
    buffer->response_ = nullptr;
  });
  download_request->run();
  return std::move(response);
}

MegaNz::MegaNz()
    : CloudProvider(util::make_unique<Auth>()),
      mega_(),
      authorized_(),
      engine_(device_()),
      daemon_(),
      temporary_directory_("."),
      deleted_(false) {}

MegaNz::~MegaNz() {
  {
    std::unique_lock<std::mutex> lock(mutex_);
    deleted_ = true;
    while (!stream_requests_.empty()) {
      {
        auto r = *stream_requests_.begin();
        stream_requests_.erase(stream_requests_.begin());
        lock.unlock();
        r->cancel();
      }
      lock.lock();
    }
  }
  daemon_ = nullptr;
  mega_ = nullptr;
}

void MegaNz::addStreamRequest(DownloadFileRequest::Pointer r) {
  std::lock_guard<std::mutex> lock(mutex_);
  stream_requests_.insert(r);
}

void MegaNz::removeStreamRequest(DownloadFileRequest::Pointer r) {
  r->cancel();
  std::lock_guard<std::mutex> lock(mutex_);
  stream_requests_.erase(r);
}

void MegaNz::addRequestListener(IRequest<EitherError<void>>::Pointer p) {
  std::lock_guard<std::mutex> lock(mutex_);
  request_listeners_.insert(p);
}

void MegaNz::removeRequestListener(IRequest<EitherError<void>>::Pointer p) {
  std::lock_guard<std::mutex> lock(mutex_);
  request_listeners_.erase(request_listeners_.find(p));
}

void MegaNz::initialize(InitData&& data) {
  {
    auto lock = auth_lock();
    if (data.hints_.find("client_id") == std::end(data.hints_))
      mega_ = util::make_unique<MegaApi>("ZVhB0Czb");
    else
      setWithHint(data.hints_, "client_id", [this](std::string v) {
        mega_ = util::make_unique<MegaApi>(v.c_str());
      });
    setWithHint(data.hints_, "temporary_directory",
                [this](std::string v) { temporary_directory_ = v; });
    setWithHint(data.hints_, "file_url",
                [this](std::string v) { file_url_ = v; });
    if (file_url_.empty()) file_url_ = DEFAULT_FILE_URL;
  }
  CloudProvider::initialize(std::move(data));
  daemon_ =
      http_server()->create(util::make_unique<HttpServerCallback>(this),
                            auth()->state(), IHttpServer::Type::FileProvider);
}

std::string MegaNz::name() const { return "mega"; }

std::string MegaNz::endpoint() const { return file_url_; }

IItem::Pointer MegaNz::rootDirectory() const {
  return util::make_unique<Item>("root", "/", IItem::FileType::Directory);
}

ICloudProvider::Hints MegaNz::hints() const {
  Hints result = {{"temporary_directory", temporary_directory_},
                  {"file_url", file_url_}};
  auto t = CloudProvider::hints();
  result.insert(t.begin(), t.end());
  return result;
}

ICloudProvider::ExchangeCodeRequest::Pointer MegaNz::exchangeCodeAsync(
    const std::string& code, ExchangeCodeCallback callback) {
  auto r = std::make_shared<Request<EitherError<Token>>>(shared_from_this());
  r->set([=](Request<EitherError<Token>>::Ptr r) {
    auto token = authorizationCodeToToken(code);
    EitherError<Token> ret =
        token->token_.empty()
            ? EitherError<Token>(
                  Error{IHttpRequest::Failure, "invalid authorization code"})
            : EitherError<Token>({token->token_, ""});
    callback(ret);
    r->done(ret);
  });
  return r->run();
}

AuthorizeRequest::Pointer MegaNz::authorizeAsync() {
  return std::make_shared<AuthorizeRequest>(
      shared_from_this(), [=](AuthorizeRequest::Pointer r,
                              AuthorizeRequest::AuthorizeCompleted complete) {
        auto fetch = [=]() {
          auto fetch_nodes_listener = std::make_shared<RequestListener>(
              [=](EitherError<void> e, Listener*) {
                if (!e.left()) authorized_ = true;
                complete(e);
              },
              this);
          r->subrequest(fetch_nodes_listener);
          mega_->fetchNodes(fetch_nodes_listener.get());
        };
        login(r, [=](EitherError<void> e) {
          if (!e.left()) return fetch();
          if (auth_callback()->userConsentRequired(*this) ==
              ICloudProvider::IAuthCallback::Status::WaitForAuthorizationCode) {
            auto code = [=](EitherError<std::string> e) {
              if (e.left()) return complete(e.left());
              {
                auto lock = auth_lock();
                auth()->set_access_token(authorizationCodeToToken(*e.right()));
              }
              login(r, [=](EitherError<void> e) {
                if (e.left())
                  complete(e.left());
                else
                  fetch();
              });
            };
            r->set_server(
                r->provider()->auth()->requestAuthorizationCode(code));
          } else {
            complete(Error{IHttpRequest::Aborted, "not waiting for code"});
          }
        });
      });
}

ICloudProvider::GetItemDataRequest::Pointer MegaNz::getItemDataAsync(
    const std::string& id, GetItemDataCallback callback) {
  auto r = std::make_shared<Request<EitherError<IItem>>>(shared_from_this());
  r->set([=](Request<EitherError<IItem>>::Ptr r) {
    ensureAuthorized<EitherError<IItem>>(r, callback, [=] {
      std::unique_ptr<mega::MegaNode> node(mega_->getNodeByPath(id.c_str()));
      if (!node) {
        Error e{IHttpRequest::NotFound, "not found"};
        callback(e);
        return r->done(e);
      }
      auto item = toItem(node.get());
      callback(item);
      return r->done(item);
    });
  });
  return r->run();
}

ICloudProvider::ListDirectoryRequest::Pointer MegaNz::listDirectoryAsync(
    IItem::Pointer item, IListDirectoryCallback::Pointer callback) {
  using ItemList = EitherError<std::vector<IItem::Pointer>>;
  auto r = std::make_shared<Request<ItemList>>(shared_from_this());
  r->set([=](Request<ItemList>::Ptr r) {
    ensureAuthorized<ItemList>(
        r, std::bind(&IListDirectoryCallback::done, callback.get(), _1), [=] {
          std::unique_ptr<mega::MegaNode> node(
              mega_->getNodeByPath(item->id().c_str()));
          if (node) {
            std::vector<IItem::Pointer> result;
            std::unique_ptr<mega::MegaNodeList> lst(
                mega_->getChildren(node.get()));
            if (lst) {
              for (int i = 0; i < lst->size(); i++) {
                auto item = toItem(lst->get(i));
                result.push_back(item);
                callback->receivedItem(item);
              }
            }
            callback->done(result);
            r->done(result);
          } else {
            Error e{IHttpRequest::NotFound, "node not found"};
            callback->done(e);
            r->done(e);
          }
        });
  });
  return r->run();
}

ICloudProvider::DownloadFileRequest::Pointer MegaNz::downloadFileAsync(
    IItem::Pointer item, IDownloadFileCallback::Pointer callback) {
  auto r = std::make_shared<Request<EitherError<void>>>(shared_from_this());
  r->set(downloadResolver(item, callback));
  return r->run();
}

ICloudProvider::UploadFileRequest::Pointer MegaNz::uploadFileAsync(
    IItem::Pointer item, const std::string& filename,
    IUploadFileCallback::Pointer callback) {
  auto r = std::make_shared<Request<EitherError<void>>>(shared_from_this());
  r->set([=](Request<EitherError<void>>::Ptr r) {
    ensureAuthorized<EitherError<void>>(
        r, std::bind(&IUploadFileCallback::done, callback.get(), _1), [=] {
          std::string cache = temporaryFileName();
          {
            std::fstream mega_cache(cache.c_str(),
                                    std::fstream::out | std::fstream::binary);
            if (!mega_cache) {
              Error e{IHttpRequest::Forbidden,
                      "couldn't open cache file" + cache};
              callback->done(e);
              return r->done(e);
            }
            std::array<char, BUFFER_SIZE> buffer;
            while (auto length =
                       callback->putData(buffer.data(), BUFFER_SIZE)) {
              if (r->is_cancelled()) {
                std::remove(cache.c_str());
                Error e{IHttpRequest::Aborted, ""};
                callback->done(e);
                return r->done(e);
              }
              mega_cache.write(buffer.data(), length);
            }
          }
          auto listener = std::make_shared<TransferListener>(
              [=](EitherError<void> e, Listener*) {
                std::remove(cache.c_str());
                callback->done(e);
                return r->done(e);
              },
              this);
          listener->upload_callback_ = callback;
          r->subrequest(listener);
          std::unique_ptr<mega::MegaNode> node(
              mega_->getNodeByPath(item->id().c_str()));
          mega_->startUpload(cache.c_str(), node.get(), filename.c_str(),
                             listener.get());

        });
  });
  return r->run();
}

ICloudProvider::DownloadFileRequest::Pointer MegaNz::getThumbnailAsync(
    IItem::Pointer item, IDownloadFileCallback::Pointer callback) {
  auto r = std::make_shared<Request<EitherError<void>>>(shared_from_this());
  r->set([=](Request<EitherError<void>>::Ptr r) {
    ensureAuthorized<EitherError<void>>(
        r, std::bind(&IDownloadFileCallback::done, callback.get(), _1), [=] {
          std::string cache = temporaryFileName();
          auto listener = std::make_shared<RequestListener>(
              [=](EitherError<void> e, Listener*) {
                if (e.left()) {
                  callback->done(e.left());
                  return r->done(e.left());
                }
                std::fstream cache_file(
                    cache.c_str(), std::fstream::in | std::fstream::binary);
                if (!cache_file) {
                  Error e{IHttpRequest::Failure,
                          "couldn't open cache file " + cache};
                  callback->done(e);
                  return r->done(e);
                }
                std::array<char, BUFFER_SIZE> buffer;
                do {
                  cache_file.read(buffer.data(), BUFFER_SIZE);
                  callback->receivedData(buffer.data(), cache_file.gcount());
                } while (cache_file.gcount() > 0);
                std::remove(cache.c_str());
                callback->done(nullptr);
                r->done(nullptr);
              },
              this);
          r->subrequest(listener);
          std::unique_ptr<mega::MegaNode> node(
              mega_->getNodeByPath(item->id().c_str()));
          mega_->getThumbnail(node.get(), cache.c_str(), listener.get());
        });
  });
  return r->run();
}

ICloudProvider::DeleteItemRequest::Pointer MegaNz::deleteItemAsync(
    IItem::Pointer item, DeleteItemCallback callback) {
  auto r = std::make_shared<Request<EitherError<void>>>(shared_from_this());
  r->set([=](Request<EitherError<void>>::Ptr r) {
    ensureAuthorized<EitherError<void>>(r, callback, [=] {
      std::unique_ptr<mega::MegaNode> node(
          mega_->getNodeByPath(item->id().c_str()));
      if (!node) {
        Error e{IHttpRequest::NotFound, "file not found"};
        callback(e);
        r->done(e);
      } else {
        auto listener = std::make_shared<RequestListener>(
            [=](EitherError<void> e, Listener*) {
              callback(e);
              return r->done(e);
            },
            this);
        r->subrequest(listener);
        mega_->remove(node.get(), listener.get());
      }
    });
  });
  return r->run();
}

ICloudProvider::CreateDirectoryRequest::Pointer MegaNz::createDirectoryAsync(
    IItem::Pointer parent, const std::string& name,
    CreateDirectoryCallback callback) {
  auto r = std::make_shared<Request<EitherError<IItem>>>(shared_from_this());
  r->set([=](Request<EitherError<IItem>>::Ptr r) {
    ensureAuthorized<EitherError<IItem>>(r, callback, [=] {
      std::unique_ptr<mega::MegaNode> parent_node(
          mega_->getNodeByPath(parent->id().c_str()));
      if (!parent_node) {
        Error e{IHttpRequest::NotFound, "parent not found"};
        callback(e);
        return r->done(e);
      }
      auto listener = std::make_shared<RequestListener>(
          [=](EitherError<void> e, Listener* listener) {
            if (e.left()) {
              callback(e.left());
              return r->done(e.left());
            }
            std::unique_ptr<mega::MegaNode> node(mega_->getNodeByHandle(
                static_cast<RequestListener*>(listener)->node_));
            auto item = toItem(node.get());
            callback(item);
            r->done(item);
          },
          this);
      r->subrequest(listener);
      mega_->createFolder(name.c_str(), parent_node.get(), listener.get());
    });
  });
  return r->run();
}

ICloudProvider::MoveItemRequest::Pointer MegaNz::moveItemAsync(
    IItem::Pointer source, IItem::Pointer destination,
    MoveItemCallback callback) {
  auto r = std::make_shared<Request<EitherError<void>>>(shared_from_this());
  r->set([=](Request<EitherError<void>>::Ptr r) {
    ensureAuthorized<EitherError<void>>(r, callback, [=] {
      std::unique_ptr<mega::MegaNode> source_node(
          mega_->getNodeByPath(source->id().c_str()));
      std::unique_ptr<mega::MegaNode> destination_node(
          mega_->getNodeByPath(destination->id().c_str()));
      if (source_node && destination_node) {
        auto listener = std::make_shared<RequestListener>(
            [=](EitherError<void> e, Listener*) {
              callback(e);
              r->done(e);
            },
            this);
        r->subrequest(listener);
        mega_->moveNode(source_node.get(), destination_node.get(),
                        listener.get());
      } else {
        Error error{IHttpRequest::Failure, "no source node / destination node"};
        callback(error);
        r->done(error);
      }
    });
  });
  return r->run();
}

ICloudProvider::RenameItemRequest::Pointer MegaNz::renameItemAsync(
    IItem::Pointer item, const std::string& name, RenameItemCallback callback) {
  auto r = std::make_shared<Request<EitherError<void>>>(shared_from_this());
  r->set([=](Request<EitherError<void>>::Ptr r) {
    ensureAuthorized<EitherError<void>>(r, callback, [=] {
      std::unique_ptr<mega::MegaNode> node(
          mega_->getNodeByPath(item->id().c_str()));
      if (node) {
        auto listener = std::make_shared<RequestListener>(
            [=](EitherError<void> e, Listener*) {
              callback(e);
              r->done(e);
            },
            this);
        r->subrequest(listener);
        mega_->renameNode(node.get(), name.c_str(), listener.get());
      } else {
        Error e{IHttpRequest::NotFound, "node not found"};
        callback(e);
        r->done(e);
      }
    });
  });
  return r->run();
}

ICloudProvider::ListDirectoryPageRequest::Pointer
MegaNz::listDirectoryPageAsync(IItem::Pointer item, const std::string&,
                               ListDirectoryPageCallback complete) {
  auto r = std::make_shared<Request<EitherError<PageData>>>(shared_from_this());
  r->set([=](Request<EitherError<PageData>>::Ptr r) {
    ensureAuthorized<EitherError<PageData>>(r, complete, [=] {
      std::unique_ptr<mega::MegaNode> node(
          mega_->getNodeByPath(item->id().c_str()));
      if (node) {
        std::vector<IItem::Pointer> result;
        std::unique_ptr<mega::MegaNodeList> lst(mega_->getChildren(node.get()));
        if (lst) {
          for (int i = 0; i < lst->size(); i++) {
            auto item = toItem(lst->get(i));
            result.push_back(item);
          }
        }
        complete(PageData{result, ""});
        r->done(PageData{result, ""});
      } else {
        Error e{IHttpRequest::NotFound, "node not found"};
        complete(e);
        r->done(e);
      }
    });
  });
  return r->run();
}

std::function<void(Request<EitherError<void>>::Ptr)> MegaNz::downloadResolver(
    IItem::Pointer item, IDownloadFileCallback::Pointer callback, int64_t start,
    int64_t size) {
  return [=](Request<EitherError<void>>::Ptr r) {
    ensureAuthorized<EitherError<void>>(
        r, std::bind(&IDownloadFileCallback::done, callback.get(), _1), [=] {
          std::unique_ptr<mega::MegaNode> node(
              mega_->getNodeByPath(item->id().c_str()));
          auto listener = std::make_shared<TransferListener>(
              [=](EitherError<void> e, Listener*) {
                callback->done(e);
                r->done(e);
              },
              this);
          listener->download_callback_ = callback;
          r->subrequest(listener);
          mega_->startStreaming(node.get(), start,
                                size == -1 ? node->getSize() - start : size,
                                listener.get());
        });
  };
}

void MegaNz::login(Request<EitherError<void>>::Ptr r,
                   AuthorizeRequest::AuthorizeCompleted complete) {
  auto auth_listener = std::make_shared<RequestListener>(
      [=](EitherError<void> e, Listener*) { complete(e); }, this);
  r->subrequest(auth_listener);
  auto data = credentialsFromString(token());
  std::string mail = data.first;
  std::string private_key = data.second;
  std::unique_ptr<char[]> hash(
      mega_->getStringHash(private_key.c_str(), mail.c_str()));
  mega_->fastLogin(mail.c_str(), hash.get(), private_key.c_str(),
                   auth_listener.get());
}

std::string MegaNz::passwordHash(const std::string& password) const {
  std::unique_ptr<char[]> hash(mega_->getBase64PwKey(password.c_str()));
  return std::string(hash.get());
}

IItem::Pointer MegaNz::toItem(MegaNode* node) {
  std::unique_ptr<char[]> path(mega_->getNodePath(node));
  auto item = util::make_unique<Item>(
      node->getName(), path.get(),
      node->isFolder() ? IItem::FileType::Directory : IItem::FileType::Unknown);
  item->set_url(endpoint() + "/?file=" + util::Url::escape(path.get()) +
                "&state=" + auth()->state());
  return std::move(item);
}

std::string MegaNz::randomString(int length) {
  std::unique_lock<std::mutex> lock(mutex_);
  std::uniform_int_distribution<char> dist('a', 'z');
  std::string result;
  for (int i = 0; i < length; i++) result += dist(engine_);
  return result;
}

std::string MegaNz::temporaryFileName() {
  return temporary_directory_ + randomString(CACHE_FILENAME_LENGTH);
}

template <class T>
void MegaNz::ensureAuthorized(typename Request<T>::Ptr r,
                              std::function<void(T)> on_error,
                              std::function<void()> on_success) {
  auto f = [=](EitherError<void> e) {
    if (e.left()) {
      on_error(e.left());
      r->done(e.left());
    } else
      on_success();
  };
  if (!authorized_)
    r->reauthorize(f);
  else
    f(nullptr);
}

IAuth::Token::Pointer MegaNz::authorizationCodeToToken(
    const std::string& code) const {
  auto data = credentialsFromString(code);
  IAuth::Token::Pointer token = util::make_unique<IAuth::Token>();
  token->token_ = data.first + Auth::SEPARATOR + passwordHash(data.second);
  token->refresh_token_ = token->token_;
  return token;
}

MegaNz::Auth::Auth() {}

std::string MegaNz::Auth::authorizeLibraryUrl() const {
  return redirect_uri() + "/login?state=" + state();
}

IHttpRequest::Pointer MegaNz::Auth::exchangeAuthorizationCodeRequest(
    std::ostream&) const {
  return nullptr;
}

IHttpRequest::Pointer MegaNz::Auth::refreshTokenRequest(std::ostream&) const {
  return nullptr;
}

IAuth::Token::Pointer MegaNz::Auth::exchangeAuthorizationCodeResponse(
    std::istream&) const {
  return nullptr;
}

IAuth::Token::Pointer MegaNz::Auth::refreshTokenResponse(std::istream&) const {
  return nullptr;
}

}  // namespace cloudstorage
