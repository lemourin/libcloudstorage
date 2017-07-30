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

const int BUFFER_SIZE = 1024;
const int CACHE_FILENAME_LENGTH = 12;
const int DEFAULT_DAEMON_PORT = 12346;

namespace cloudstorage {

namespace {

class Listener : public IRequest<EitherError<void>> {
 public:
  static constexpr int IN_PROGRESS = -1;
  static constexpr int FAILURE = 0;
  static constexpr int SUCCESS = 1;
  static constexpr int CANCELLED = 2;

  Listener() : status_(IN_PROGRESS), code_(IHttpRequest::Unknown) {}

  void cancel() override {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (status_ == CANCELLED) return;
      status_ = CANCELLED;
      code_ = IHttpRequest::Aborted;
    }
    condition_.notify_all();
    finish();
  }

  EitherError<void> result() override {
    finish();
    std::lock_guard<std::mutex> lock(mutex_);
    if (status_ != SUCCESS)
      return Error{code_, error_};
    else
      return nullptr;
  }

 protected:
  int status_;
  std::mutex mutex_;
  std::condition_variable condition_;
  int code_;
  std::string error_;
};

class RequestListener : public mega::MegaRequestListener, public Listener {
 public:
  void finish() override {
    std::unique_lock<std::mutex> lock(mutex_);
    condition_.wait(lock, [this]() { return status_ != IN_PROGRESS; });
  }

  void onRequestFinish(MegaApi*, MegaRequest* r, MegaError* e) override {
    if (e->getErrorCode() == 0)
      status_ = SUCCESS;
    else {
      status_ = FAILURE;
      code_ = e->getErrorCode();
      error_ = e->getErrorString();
    }
    if (r->getLink()) link_ = r->getLink();
    node_ = r->getNodeHandle();
    condition_.notify_all();
  }

  std::string link_;
  MegaHandle node_;
};

class TransferListener : public mega::MegaTransferListener, public Listener {
 public:
  void finish() override {
    std::unique_lock<std::mutex> lock(mutex_);
    condition_.wait(lock, [this]() {
      return status_ != IN_PROGRESS && status_ != CANCELLED;
    });
  }

  bool onTransferData(MegaApi*, MegaTransfer* t, char* buffer,
                      size_t size) override {
    if (status_ == CANCELLED) return false;
    if (download_callback_) {
      download_callback_->receivedData(buffer, size);
      download_callback_->progress(t->getTotalBytes(),
                                   t->getTransferredBytes());
    }
    return true;
  }

  void onTransferUpdate(MegaApi* mega, MegaTransfer* t) override {
    if (upload_callback_)
      upload_callback_->progress(t->getTotalBytes(), t->getTransferredBytes());
    if (status_ == CANCELLED) mega->cancelTransfer(t);
  }

  void onTransferFinish(MegaApi*, MegaTransfer*, MegaError* e) override {
    if (e->getErrorCode() == 0)
      status_ = SUCCESS;
    else {
      code_ = e->getErrorCode();
      error_ = e->getErrorString();
      status_ = FAILURE;
    }
    condition_.notify_all();
  }

  IDownloadFileCallback::Pointer download_callback_;
  IUploadFileCallback::Pointer upload_callback_;
};

struct Buffer {
  using Pointer = std::shared_ptr<Buffer>;

  std::mutex mutex_;
  std::queue<char> data_;
};

class HttpData : public IHttpServer::IResponse::ICallback {
 public:
  HttpData(Buffer::Pointer d) : buffer_(d) {}

  ~HttpData() {
    auto p = request_->provider();
    if (p) {
      auto mega = static_cast<MegaNz*>(request_->provider().get());
      mega->removeStreamRequest(request_);
    }
  }

  int putData(char* buf, size_t max) override {
    if (request_->is_cancelled()) return -1;
    std::lock_guard<std::mutex> lock(buffer_->mutex_);
    size_t cnt = std::min(buffer_->data_.size(), max);
    for (size_t i = 0; i < cnt; i++) {
      buf[i] = buffer_->data_.front();
      buffer_->data_.pop();
    }
    return cnt;
  }

  Buffer::Pointer buffer_;
  std::shared_ptr<Request<EitherError<void>>> request_;
};

class HttpDataCallback : public IDownloadFileCallback {
 public:
  HttpDataCallback(Buffer::Pointer d) : buffer_(d) {}

  void receivedData(const char* data, uint32_t length) override {
    std::lock_guard<std::mutex> lock(buffer_->mutex_);
    for (uint32_t i = 0; i < length; i++) buffer_->data_.push(data[i]);
  }

  void done(EitherError<void>) override {}
  void progress(uint32_t, uint32_t) override {}

  Buffer::Pointer buffer_;
};

}  // namespace

MegaNz::HttpServerCallback::HttpServerCallback(MegaNz* p) : provider_(p) {}

IHttpServer::IResponse::Pointer MegaNz::HttpServerCallback::receivedConnection(
    const IHttpServer& server, const IHttpServer::IConnection& connection) {
  const char* state = connection.getParameter("state");
  if (!state || state != provider_->auth()->state())
    return server.createResponse(403, {}, "state parameter missing / invalid");
  const char* file = connection.getParameter("file");
  std::unique_ptr<mega::MegaNode> node(provider_->mega()->getNodeByHandle(
      provider_->mega()->base64ToHandle(file)));
  if (!node) return server.createResponse(404, {}, "file not found");
  auto buffer = std::make_shared<Buffer>();
  auto data = util::make_unique<HttpData>(buffer);
  auto request = std::make_shared<Request<EitherError<void>>>(
      std::weak_ptr<CloudProvider>(provider_->shared_from_this()));
  data->request_ = request;
  provider_->addStreamRequest(request);
  int code = 200;
  auto extension =
      static_cast<Item*>(provider_->toItem(node.get()).get())->extension();
  std::unordered_map<std::string, std::string> headers = {
      {"Content-Type", util::to_mime_type(extension)},
      {"Accept-Ranges", "bytes"},
      {"Content-Disposition",
       "inline; filename=\"" + std::string(node->getName()) + "\""}};
  util::range range = {0, node->getSize()};
  if (const char* range_str = connection.header("Range")) {
    range = util::parse_range(range_str);
    if (range.size == -1) range.size = node->getSize() - range.start;
    if (range.start + range.size > node->getSize() || range.start == -1)
      return server.createResponse(416, {}, "invalid range");
    std::stringstream stream;
    stream << "bytes " << range.start << "-" << range.start + range.size - 1
           << "/" << node->getSize();
    headers["Content-Range"] = stream.str();
    code = 206;
  }
  request->set_resolver(provider_->downloadResolver(
      provider_->toItem(node.get()),
      util::make_unique<HttpDataCallback>(buffer), range.start, range.size));
  return server.createResponse(code, headers, range.size, BUFFER_SIZE,
                               std::move(data));
}

MegaNz::MegaNz()
    : CloudProvider(util::make_unique<Auth>()),
      mega_(),
      authorized_(),
      engine_(device_()),
      daemon_port_(DEFAULT_DAEMON_PORT),
      daemon_(),
      temporary_directory_(".") {}

MegaNz::~MegaNz() {
  daemon_ = nullptr;
  std::unordered_set<DownloadFileRequest::Pointer> requests;
  {
    std::unique_lock<std::mutex> lock(mutex_);
    requests = stream_requests_;
    stream_requests_.clear();
  }
  for (auto r : requests) r->cancel();
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

void MegaNz::initialize(InitData&& data) {
  {
    std::lock_guard<std::mutex> lock(auth_mutex());
    if (data.hints_.find("client_id") == std::end(data.hints_))
      mega_ = util::make_unique<MegaApi>("ZVhB0Czb");
    else
      setWithHint(data.hints_, "client_id", [this](std::string v) {
        mega_ = util::make_unique<MegaApi>(v.c_str());
      });
    setWithHint(data.hints_, "daemon_port",
                [this](std::string v) { daemon_port_ = std::atoi(v.c_str()); });
    setWithHint(data.hints_, "temporary_directory",
                [this](std::string v) { temporary_directory_ = v; });
    setWithHint(data.hints_, "file_url",
                [this](std::string v) { file_url_ = v; });
  }
  CloudProvider::initialize(std::move(data));
  if (file_url_.empty()) file_url_ = auth()->redirect_uri_host();
  daemon_ = http_server()->create(
      util::make_unique<HttpServerCallback>(this), auth()->state(),
      IHttpServer::Type::MultiThreaded, daemon_port_);
}

std::string MegaNz::name() const { return "mega"; }

std::string MegaNz::endpoint() const {
  return util::address(file_url_, daemon_port_);
}

IItem::Pointer MegaNz::rootDirectory() const {
  return util::make_unique<Item>("root", "/", IItem::FileType::Directory);
}

ICloudProvider::Hints MegaNz::hints() const {
  Hints result = {{"daemon_port", std::to_string(daemon_port_)},
                  {"temporary_directory", temporary_directory_}};
  auto t = CloudProvider::hints();
  result.insert(t.begin(), t.end());
  return result;
}

ICloudProvider::ExchangeCodeRequest::Pointer MegaNz::exchangeCodeAsync(
    const std::string& code, ExchangeCodeCallback callback) {
  auto r =
      util::make_unique<Request<EitherError<std::string>>>(shared_from_this());
  r->set_resolver([this, code, callback](Request<EitherError<std::string>>*) {
    auto token = authorizationCodeToToken(code);
    EitherError<std::string> ret =
        token->token_.empty()
            ? EitherError<std::string>(Error{500, "invalid authorization code"})
            : EitherError<std::string>(token->token_);
    callback(ret);
    return ret;
  });
  return std::move(r);
}

AuthorizeRequest::Pointer MegaNz::authorizeAsync() {
  return util::make_unique<Authorize>(
      shared_from_this(), [this](AuthorizeRequest* r) -> EitherError<void> {
        if (login(r).left()) {
          if (r->is_cancelled()) return Error{IHttpRequest::Aborted, ""};
          if (callback()->userConsentRequired(*this) ==
              ICloudProvider::ICallback::Status::WaitForAuthorizationCode) {
            auto code = r->getAuthorizationCode();
            {
              std::lock_guard<std::mutex> mutex(auth_mutex());
              auth()->set_access_token(authorizationCodeToToken(code));
            }
            auto result = login(r);
            if (result.left()) return result.left();
          }
        }
        auto fetch_nodes_listener = std::make_shared<RequestListener>();
        r->subrequest(fetch_nodes_listener);
        mega_->fetchNodes(fetch_nodes_listener.get());
        auto result = fetch_nodes_listener->result();
        mega_->removeRequestListener(fetch_nodes_listener.get());
        if (!result.left()) authorized_ = true;
        return result;
      });
}

ICloudProvider::GetItemDataRequest::Pointer MegaNz::getItemDataAsync(
    const std::string& id, GetItemDataCallback callback) {
  auto r = util::make_unique<Request<EitherError<IItem>>>(shared_from_this());
  r->set_resolver([id, callback,
                   this](Request<EitherError<IItem>>* r) -> EitherError<IItem> {
    if (!ensureAuthorized(r)) {
      Error e{401, "unauthorized"};
      callback(e);
      return e;
    }
    std::unique_ptr<mega::MegaNode> node(mega_->getNodeByPath(id.c_str()));
    if (!node) {
      Error e{404, "not found"};
      callback(e);
      return e;
    }
    auto item = toItem(node.get());
    callback(item);
    return item;
  });
  return std::move(r);
}

ICloudProvider::ListDirectoryRequest::Pointer MegaNz::listDirectoryAsync(
    IItem::Pointer item, IListDirectoryCallback::Pointer callback) {
  auto r = util::make_unique<Request<EitherError<std::vector<IItem::Pointer>>>>(
      shared_from_this());
  r->set_resolver([this, item, callback](
                      Request<EitherError<std::vector<IItem::Pointer>>>* r)
                      -> EitherError<std::vector<IItem::Pointer>> {
    if (!ensureAuthorized(r)) {
      Error e = r->is_cancelled() ? Error{IHttpRequest::Aborted, ""}
                                  : Error{401, "authorization failed"};
      callback->done(e);
      return e;
    }
    std::unique_ptr<mega::MegaNode> node(
        mega_->getNodeByPath(item->id().c_str()));
    std::vector<IItem::Pointer> result;
    if (node) {
      std::unique_ptr<mega::MegaNodeList> lst(mega_->getChildren(node.get()));
      if (lst) {
        for (int i = 0; i < lst->size(); i++) {
          auto item = toItem(lst->get(i));
          result.push_back(item);
          callback->receivedItem(item);
        }
      }
    } else {
      Error e{404, "node not found"};
      callback->done(e);
      return e;
    }
    callback->done(result);
    return result;
  });
  return std::move(r);
}

ICloudProvider::DownloadFileRequest::Pointer MegaNz::downloadFileAsync(
    IItem::Pointer item, IDownloadFileCallback::Pointer callback) {
  auto r = util::make_unique<Request<EitherError<void>>>(shared_from_this());
  r->set_resolver(downloadResolver(item, callback));
  return std::move(r);
}

ICloudProvider::UploadFileRequest::Pointer MegaNz::uploadFileAsync(
    IItem::Pointer item, const std::string& filename,
    IUploadFileCallback::Pointer callback) {
  auto r = util::make_unique<Request<EitherError<void>>>(shared_from_this());
  r->set_resolver([this, item, callback, filename](
                      Request<EitherError<void>>* r) -> EitherError<void> {
    if (!ensureAuthorized(r)) {
      Error e = r->is_cancelled() ? Error{IHttpRequest::Aborted, ""}
                                  : Error{401, "authorization failed"};
      callback->done(e);
      return e;
    }
    std::string cache = temporaryFileName();
    {
      std::fstream mega_cache(cache.c_str(),
                              std::fstream::out | std::fstream::binary);
      if (!mega_cache) {
        Error e{403, "couldn't open cache file" + cache};
        callback->done(e);
        return e;
      }
      std::array<char, BUFFER_SIZE> buffer;
      while (auto length = callback->putData(buffer.data(), BUFFER_SIZE)) {
        if (r->is_cancelled()) {
          std::remove(cache.c_str());
          Error e{IHttpRequest::Aborted, ""};
          callback->done(e);
          return e;
        }
        mega_cache.write(buffer.data(), length);
      }
    }
    auto listener = std::make_shared<TransferListener>();
    listener->upload_callback_ = callback;
    r->subrequest(listener);
    std::unique_ptr<mega::MegaNode> node(
        mega_->getNodeByPath(item->id().c_str()));
    mega_->startUpload(cache.c_str(), node.get(), filename.c_str(),
                       listener.get());
    auto result = listener->result();
    std::remove(cache.c_str());
    mega_->removeTransferListener(listener.get());
    callback->done(result);
    return result;
  });
  return std::move(r);
}

ICloudProvider::DownloadFileRequest::Pointer MegaNz::getThumbnailAsync(
    IItem::Pointer item, IDownloadFileCallback::Pointer callback) {
  auto r = util::make_unique<Request<EitherError<void>>>(shared_from_this());
  r->set_resolver([this, item, callback](
                      Request<EitherError<void>>* r) -> EitherError<void> {
    if (!ensureAuthorized(r)) {
      Error e = r->is_cancelled() ? Error{IHttpRequest::Aborted, ""}
                                  : Error{401, "authorization failed"};
      callback->done(e);
      return e;
    }
    auto listener = std::make_shared<RequestListener>();
    r->subrequest(listener);
    std::string cache = temporaryFileName();
    std::unique_ptr<mega::MegaNode> node(
        mega_->getNodeByPath(item->id().c_str()));
    mega_->getThumbnail(node.get(), cache.c_str(), listener.get());
    auto result = listener->result();
    mega_->removeRequestListener(listener.get());
    if (!result.left()) {
      std::fstream cache_file(cache.c_str(),
                              std::fstream::in | std::fstream::binary);
      if (!cache_file) {
        Error e{500, "couldn't open cache file " + cache};
        callback->done(e);
        return e;
      }
      std::array<char, BUFFER_SIZE> buffer;
      do {
        cache_file.read(buffer.data(), BUFFER_SIZE);
        callback->receivedData(buffer.data(), cache_file.gcount());
      } while (cache_file.gcount() > 0);
      std::remove(cache.c_str());
      callback->done(nullptr);
      return nullptr;
    } else {
      auto data = cloudstorage::DownloadFileRequest::generateThumbnail(r, item);
      if (data.right()) {
        callback->receivedData(data.right()->data(), data.right()->size());
        callback->done(nullptr);
        return nullptr;
      } else {
        callback->done(data.left());
        return data.left();
      }
    }
  });
  return std::move(r);
}

ICloudProvider::DeleteItemRequest::Pointer MegaNz::deleteItemAsync(
    IItem::Pointer item, DeleteItemCallback callback) {
  auto r = util::make_unique<Request<EitherError<void>>>(shared_from_this());
  r->set_resolver([this, item, callback](
                      Request<EitherError<void>>* r) -> EitherError<void> {
    if (!ensureAuthorized(r)) {
      Error e = r->is_cancelled() ? Error{IHttpRequest::Aborted, ""}
                                  : Error{401, "authorization failed"};
      callback(e);
      return e;
    }
    std::unique_ptr<mega::MegaNode> node(
        mega_->getNodeByPath(item->id().c_str()));
    auto listener = std::make_shared<RequestListener>();
    r->subrequest(listener);
    mega_->remove(node.get(), listener.get());
    auto result = listener->result();
    mega_->removeRequestListener(listener.get());
    callback(result);
    return result;
  });
  return std::move(r);
}

ICloudProvider::CreateDirectoryRequest::Pointer MegaNz::createDirectoryAsync(
    IItem::Pointer parent, const std::string& name,
    CreateDirectoryCallback callback) {
  auto r = util::make_unique<Request<EitherError<IItem>>>(shared_from_this());
  r->set_resolver([=](Request<EitherError<IItem>>* r) -> EitherError<IItem> {
    if (!ensureAuthorized(r)) {
      Error e = r->is_cancelled() ? Error{IHttpRequest::Aborted, ""}
                                  : Error{401, "authorization failed"};
      callback(e);
      return e;
    }
    std::unique_ptr<mega::MegaNode> parent_node(
        mega_->getNodeByPath(parent->id().c_str()));
    if (!parent_node) {
      Error e{404, "parent not found"};
      callback(e);
      return e;
    }
    auto listener = std::make_shared<RequestListener>();
    r->subrequest(listener);
    mega_->createFolder(name.c_str(), parent_node.get(), listener.get());
    auto result = listener->result();
    mega_->removeRequestListener(listener.get());
    if (!result.left()) {
      std::unique_ptr<mega::MegaNode> node(
          mega_->getNodeByHandle(listener->node_));
      auto item = toItem(node.get());
      callback(item);
      return item;
    } else {
      callback(result.left());
      return result.left();
    }
  });
  return std::move(r);
}

ICloudProvider::MoveItemRequest::Pointer MegaNz::moveItemAsync(
    IItem::Pointer source, IItem::Pointer destination,
    MoveItemCallback callback) {
  auto r = util::make_unique<Request<EitherError<void>>>(shared_from_this());
  r->set_resolver([=](Request<EitherError<void>>* r) -> EitherError<void> {
    if (!ensureAuthorized(r)) {
      Error e = r->is_cancelled() ? Error{IHttpRequest::Aborted, ""}
                                  : Error{401, "authorization failed"};
      callback(e);
      return e;
    }
    std::unique_ptr<mega::MegaNode> source_node(
        mega_->getNodeByPath(source->id().c_str()));
    std::unique_ptr<mega::MegaNode> destination_node(
        mega_->getNodeByPath(destination->id().c_str()));
    if (source_node && destination_node) {
      auto listener = std::make_shared<RequestListener>();
      r->subrequest(listener);
      mega_->moveNode(source_node.get(), destination_node.get(),
                      listener.get());
      auto result = listener->result();
      mega_->removeRequestListener(listener.get());
      callback(result);
      return result;
    }
    Error error{500, "no source node / destination node"};
    callback(error);
    return error;
  });
  return std::move(r);
}

ICloudProvider::RenameItemRequest::Pointer MegaNz::renameItemAsync(
    IItem::Pointer item, const std::string& name, RenameItemCallback callback) {
  auto r = util::make_unique<Request<EitherError<void>>>(shared_from_this());
  r->set_resolver([=](Request<EitherError<void>>* r) -> EitherError<void> {
    if (!ensureAuthorized(r)) {
      Error e = r->is_cancelled() ? Error{IHttpRequest::Aborted, ""}
                                  : Error{401, "authorization failed"};
      callback(e);
      return e;
    }
    std::unique_ptr<mega::MegaNode> node(
        mega_->getNodeByPath(item->id().c_str()));
    if (node) {
      auto listener = std::make_shared<RequestListener>();
      r->subrequest(listener);
      mega_->renameNode(node.get(), name.c_str(), listener.get());
      auto result = listener->result();
      mega_->removeRequestListener(listener.get());
      callback(result);
      return result;
    }
    Error e{500, "node not found"};
    callback(e);
    return e;
  });
  return std::move(r);
}

std::function<EitherError<void>(Request<EitherError<void>>*)>
MegaNz::downloadResolver(IItem::Pointer item,
                         IDownloadFileCallback::Pointer callback, int64_t start,
                         int64_t size) {
  return [this, item, callback, start,
          size](Request<EitherError<void>>* r) -> EitherError<void> {
    if (!ensureAuthorized(r)) {
      Error e = r->is_cancelled() ? Error{IHttpRequest::Aborted, ""}
                                  : Error{401, "authorization failed"};
      callback->done(e);
      return e;
    }
    std::unique_ptr<mega::MegaNode> node(
        mega_->getNodeByPath(item->id().c_str()));
    auto listener = std::make_shared<TransferListener>();
    listener->download_callback_ = callback;
    r->subrequest(listener);
    mega_->startStreaming(node.get(), start,
                          size == -1 ? node->getSize() - start : size,
                          listener.get());
    auto result = listener->result();
    mega_->removeTransferListener(listener.get());
    callback->done(result);
    return result;
  };
}

EitherError<void> MegaNz::login(Request<EitherError<void>>* r) {
  auto auth_listener = std::make_shared<RequestListener>();
  r->subrequest(auth_listener);
  auto data = credentialsFromString(token());
  std::string mail = data.first;
  std::string private_key = data.second;
  std::unique_ptr<char[]> hash(
      mega_->getStringHash(private_key.c_str(), mail.c_str()));
  mega_->fastLogin(mail.c_str(), hash.get(), private_key.c_str(),
                   auth_listener.get());
  auto result = auth_listener->result();
  mega_->removeRequestListener(auth_listener.get());
  return result;
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
  std::unique_ptr<char[]> handle(node->getBase64Handle());
  item->set_url(endpoint() + "/?file=" + handle.get() +
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
bool MegaNz::ensureAuthorized(Request<T>* r) {
  if (!authorized_)
    return r->reauthorize();
  else
    return true;
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

MegaNz::Authorize::~Authorize() { cancel(); }

}  // namespace cloudstorage
