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
#include "Request/Request.h"
#include "Utility/Item.h"
#include "Utility/Utility.h"

#include <fstream>
#include <queue>

using namespace mega;

const int BUFFER_SIZE = 1024;
const std::string SEPARATOR = "##";
const int CACHE_FILENAME_LENGTH = 12;
const int DAEMON_PORT = 12346;

namespace cloudstorage {

namespace {

class Listener {
 public:
  static constexpr int IN_PROGRESS = -1;
  static constexpr int FAILURE = 0;
  static constexpr int SUCCESS = 1;

  Listener(Semaphore* semaphore)
      : semaphore_(semaphore), status_(IN_PROGRESS) {}

  Semaphore* semaphore_;
  std::atomic_int status_;
};

class RequestListener : public mega::MegaRequestListener, public Listener {
 public:
  using Listener::Listener;

  void onRequestFinish(MegaApi*, MegaRequest* r, MegaError* e) {
    if (e->getErrorCode() == 0)
      status_ = SUCCESS;
    else
      status_ = FAILURE;
    if (r->getLink()) link_ = r->getLink();
    semaphore_->notify();
  }

  std::string link_;
};

class TransferListener : public mega::MegaTransferListener, public Listener {
 public:
  using Listener::Listener;

  bool onTransferData(MegaApi*, MegaTransfer* t, char* buffer, size_t size) {
    if (download_callback_) {
      download_callback_->receivedData(buffer, size);
      download_callback_->progress(t->getTotalBytes(),
                                   t->getTransferredBytes());
    }
    return !request_->is_cancelled();
  }

  void onTransferUpdate(MegaApi* mega, MegaTransfer* t) {
    if (upload_callback_)
      upload_callback_->progress(t->getTotalBytes(), t->getTransferredBytes());
    if (request_->is_cancelled()) mega->cancelTransfer(t);
  }

  void onTransferFinish(MegaApi*, MegaTransfer*, MegaError* e) {
    if (e->getErrorCode() == 0)
      status_ = SUCCESS;
    else
      status_ = FAILURE;
    semaphore_->notify();
  }

  IDownloadFileCallback::Pointer download_callback_;
  IUploadFileCallback::Pointer upload_callback_;
  Request<void>* request_;
};

struct HttpData {
  HttpData() : done_(), waiting_() {}

  std::mutex mutex_;
  std::queue<char> buffer_;
  std::atomic_bool done_;
  std::atomic_bool waiting_;
  Semaphore data_available_;
  ICloudProvider::DownloadFileRequest::Pointer request_;
};

class DownloadFileCallback : public IDownloadFileCallback {
 public:
  DownloadFileCallback(HttpData* data) : data_(data) {}
  ~DownloadFileCallback() { data_->data_available_.notify(); }

  virtual void receivedData(const char* data, uint32_t length) {
    std::lock_guard<std::mutex> lock(data_->mutex_);
    for (uint32_t i = 0; i < length; i++) data_->buffer_.push(data[i]);
    if (data_->waiting_ && !data_->buffer_.empty()) {
      data_->waiting_ = false;
      data_->data_available_.notify();
    }
  }

  virtual void done() {
    std::lock_guard<std::mutex> lock(data_->mutex_);
    data_->done_ = true;
  }

  virtual void error(const std::string&) {}
  virtual void progress(uint32_t, uint32_t) {}

  HttpData* data_;
};

int httpRequestCallback(void* cls, MHD_Connection* connection, const char*,
                        const char* /*method*/, const char* /*version*/,
                        const char* /*upload_data*/,
                        size_t* /*upload_data_size*/, void** /*ptr*/) {
  MegaNz* provider = static_cast<MegaNz*>(cls);
  const char* file =
      MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "file");
  auto node =
      provider->mega()->getNodeByHandle(provider->mega()->base64ToHandle(file));
  if (!node) return MHD_NO;
  auto data_provider = [](void* cls, uint64_t, char* buf,
                          size_t max) -> long int {
    HttpData* data = static_cast<HttpData*>(cls);
    bool wait = false;
    {
      std::lock_guard<std::mutex> lock(data->mutex_);
      if (data->buffer_.empty() && !data->done_) wait = true;
    }
    if (wait) {
      data->waiting_ = true;
      data->data_available_.wait();
    }
    std::lock_guard<std::mutex> lock(data->mutex_);
    size_t cnt = std::min(data->buffer_.size(), max);
    for (size_t i = 0; i < cnt; i++) {
      buf[i] = data->buffer_.front();
      data->buffer_.pop();
    }
    if (data->done_ && data->buffer_.empty())
      return MHD_CONTENT_READER_END_OF_STREAM;
    return cnt;
  };
  auto release_data = [](void* cls) {
    HttpData* data = static_cast<HttpData*>(cls);
    delete data;
  };

  HttpData* data = new HttpData;
  data->request_ = provider->downloadFileAsync(
      provider->toItem(node), make_unique<DownloadFileCallback>(data));
  MHD_Response* response = MHD_create_response_from_callback(
      MHD_SIZE_UNKNOWN, BUFFER_SIZE, data_provider, data, release_data);
  int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
  MHD_destroy_response(response);
  return ret;
}

}  // namespace

MegaNz::MegaNz()
    : CloudProvider(make_unique<Auth>()),
      mega_(),
      authorized_(),
      engine_(device_()),
      daemon_() {}

MegaNz::~MegaNz() {
  if (daemon_) MHD_stop_daemon(daemon_);
}

void MegaNz::initialize(const std::string& token,
                        ICloudProvider::ICallback::Pointer callback,
                        const ICloudProvider::Hints& hints) {
  CloudProvider::initialize(token, std::move(callback), hints);

  std::lock_guard<std::mutex> lock(auth_mutex());
  if (auth()->client_id().empty())
    mega_ = make_unique<MegaApi>("ZVhB0Czb");
  else
    mega_ = make_unique<MegaApi>(auth()->client_id().c_str());
  daemon_ = MHD_start_daemon(MHD_USE_THREAD_PER_CONNECTION, DAEMON_PORT, NULL,
                             NULL, &httpRequestCallback, this, MHD_OPTION_END);
}

std::string MegaNz::name() const { return "mega"; }

IItem::Pointer MegaNz::rootDirectory() const {
  return make_unique<Item>("root", "/", IItem::FileType::Directory);
}

AuthorizeRequest::Pointer MegaNz::authorizeAsync() {
  return make_unique<Authorize>(
      shared_from_this(), [this](AuthorizeRequest* r) {
        if (!login(r)) {
          if (r->is_cancelled()) return false;
          if (callback()->userConsentRequired(*this) ==
              ICloudProvider::ICallback::Status::WaitForAuthorizationCode) {
            std::string code = r->getAuthorizationCode();
            auto data = creditentialsFromString(code);
            {
              std::lock_guard<std::mutex> mutex(auth_mutex());
              IAuth::Token::Pointer token = make_unique<IAuth::Token>();
              token->token_ =
                  data.first + SEPARATOR + passwordHash(data.second);
              token->refresh_token_ = token->token_;
              auth()->set_access_token(std::move(token));
            }
            if (!login(r)) return false;
          }
        }
        Authorize::Semaphore semaphore(r);
        RequestListener fetch_nodes_listener_(&semaphore);
        mega_->fetchNodes(&fetch_nodes_listener_);
        semaphore.wait();
        mega_->removeRequestListener(&fetch_nodes_listener_);
        if (fetch_nodes_listener_.status_ != RequestListener::SUCCESS) {
          return false;
        }
        authorized_ = true;
        return true;
      });
}

ICloudProvider::GetItemDataRequest::Pointer MegaNz::getItemDataAsync(
    const std::string& id, GetItemDataCallback callback) {
  auto r = std::make_shared<Request<IItem::Pointer>>(shared_from_this());
  r->set_resolver(
      [id, callback, this](Request<IItem::Pointer>* r) -> IItem::Pointer {
        auto node = mega_->getNodeByPath(id.c_str());
        if (!node) {
          callback(nullptr);
          return nullptr;
        }
        auto item = toItem(node);
        Request<IItem::Pointer>::Semaphore semaphore(r);
        RequestListener listener(&semaphore);
        mega_->exportNode(node, &listener);
        semaphore.wait();
        mega_->removeRequestListener(&listener);
        if (listener.status_ == RequestListener::SUCCESS) {
          std::unique_ptr<char> handle(node->getBase64Handle());
          static_cast<Item*>(item.get())
              ->set_url("http://localhost:" + std::to_string(DAEMON_PORT) +
                        "?file=" + handle.get());
        }
        callback(item);
        return item;
      });
  return r;
}

ICloudProvider::ListDirectoryRequest::Pointer MegaNz::listDirectoryAsync(
    IItem::Pointer item, IListDirectoryCallback::Pointer callback) {
  auto r =
      make_unique<Request<std::vector<IItem::Pointer>>>(shared_from_this());
  r->set_resolver([this, item, callback](Request<std::vector<IItem::Pointer>>*
                                             r) -> std::vector<IItem::Pointer> {
    if (!authorized_) r->reauthorize();
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
    }
    callback->done(result);
    return result;
  });
  return r;
}

ICloudProvider::DownloadFileRequest::Pointer MegaNz::downloadFileAsync(
    IItem::Pointer item, IDownloadFileCallback::Pointer callback) {
  auto r = make_unique<Request<void>>(shared_from_this());
  r->set_resolver([this, item, callback](Request<void>* r) {
    if (!authorized_) r->reauthorize();
    auto node = mega_->getNodeByPath(item->id().c_str());
    Request<void>::Semaphore semaphore(r);
    TransferListener listener(&semaphore);
    listener.download_callback_ = callback;
    listener.request_ = r;
    mega_->startStreaming(node, 0, node->getSize(), &listener);
    semaphore.wait();
    if (r->is_cancelled())
      while (listener.status_ == Listener::IN_PROGRESS) semaphore.wait();
    mega_->removeTransferListener(&listener);
    if (listener.status_ != Listener::SUCCESS) {
      if (r->is_cancelled()) callback->error("Failed to download");
    } else
      callback->done();
  });
  return r;
}

ICloudProvider::UploadFileRequest::Pointer MegaNz::uploadFileAsync(
    IItem::Pointer item, const std::string& filename,
    IUploadFileCallback::Pointer callback) {
  auto r = make_unique<Request<void>>(shared_from_this());
  r->set_resolver([this, item, callback, filename](Request<void>* r) {
    if (!authorized_) r->reauthorize();
    std::string cache = randomString(CACHE_FILENAME_LENGTH);
    {
      std::fstream mega_cache(cache.c_str(),
                              std::fstream::out | std::fstream::binary);
      std::array<char, BUFFER_SIZE> buffer;
      while (auto length = callback->putData(buffer.data(), BUFFER_SIZE)) {
        if (r->is_cancelled()) return;
        mega_cache.write(buffer.data(), length);
      }
    }
    Request<void>::Semaphore semaphore(r);
    TransferListener listener(&semaphore);
    listener.upload_callback_ = callback;
    listener.request_ = r;
    mega_->startUpload(cache.c_str(), mega_->getNodeByPath(item->id().c_str()),
                       filename.c_str(), &listener);
    semaphore.wait();
    if (r->is_cancelled())
      while (listener.status_ == Listener::IN_PROGRESS) semaphore.wait();
    mega_->removeTransferListener(&listener);
    if (listener.status_ != Listener::SUCCESS)
      callback->error("Failed to upload");
    else
      callback->done();
    std::remove(cache.c_str());
  });
  return r;
}

ICloudProvider::DownloadFileRequest::Pointer MegaNz::getThumbnailAsync(
    IItem::Pointer item, IDownloadFileCallback::Pointer callback) {
  auto r = make_unique<Request<void>>(shared_from_this());
  r->set_resolver([this, item, callback](Request<void>* r) {
    Request<void>::Semaphore semaphore(r);
    RequestListener listener(&semaphore);
    std::string cache = randomString(CACHE_FILENAME_LENGTH);
    auto node = mega_->getNodeByPath(item->id().c_str());
    mega_->getThumbnail(node, cache.c_str(), &listener);
    semaphore.wait();
    if (r->is_cancelled())
      while (listener.status_ == Listener::IN_PROGRESS) semaphore.wait();
    mega_->removeRequestListener(&listener);
    if (listener.status_ == Listener::SUCCESS) {
      std::fstream cache_file(cache.c_str(),
                              std::fstream::in | std::fstream::binary);
      std::array<char, BUFFER_SIZE> buffer;
      do {
        cache_file.read(buffer.data(), BUFFER_SIZE);
        callback->receivedData(buffer.data(), cache_file.gcount());
      } while (cache_file.gcount() > 0);
      callback->done();
    } else if (listener.status_ == Listener::FAILURE) {
      callback->error("Failed to get thumbnail");
    }
    std::remove(cache.c_str());
  });
  return r;
}

std::pair<std::string, std::string> MegaNz::creditentialsFromString(
    const std::string& str) const {
  auto it = str.find(SEPARATOR);
  if (it == std::string::npos) return {};
  std::string login(str.begin(), str.begin() + it);
  std::string password(str.begin() + it + SEPARATOR.length(), str.end());
  return {login, password};
}

bool MegaNz::login(Request<bool>* r) {
  Authorize::Semaphore semaphore(r);
  RequestListener auth_listener(&semaphore);
  auto data = creditentialsFromString(access_token());
  std::string mail = data.first;
  std::string private_key = data.second;
  std::unique_ptr<char> hash(
      mega_->getStringHash(private_key.c_str(), mail.c_str()));
  mega_->fastLogin(mail.c_str(), hash.get(), private_key.c_str(),
                   &auth_listener);
  semaphore.wait();
  mega_->removeRequestListener(&auth_listener);
  return auth_listener.status_ == Listener::SUCCESS;
}

std::string MegaNz::passwordHash(const std::string& password) {
  std::unique_ptr<char> hash(mega_->getBase64PwKey(password.c_str()));
  return std::string(hash.get());
}

IItem::Pointer MegaNz::toItem(MegaNode* node) {
  std::unique_ptr<char> path(mega_->getNodePath(node));
  auto item = std::make_shared<Item>(
      node->getName(), path.get(),
      node->isFolder() ? IItem::FileType::Directory : IItem::FileType::Unknown);
  return item;
}

std::string MegaNz::randomString(int length) {
  std::unique_lock<std::mutex> lock(mutex_);
  std::uniform_int_distribution<char> dist('a', 'z');
  std::string result;
  for (int i = 0; i < length; i++) result += dist(engine_);
  return result;
}

MegaNz::Auth::Auth() {}

std::string MegaNz::Auth::authorizeLibraryUrl() const {
  return redirect_uri() + "/login";
}

HttpRequest::Pointer MegaNz::Auth::exchangeAuthorizationCodeRequest(
    std::ostream&) const {
  return nullptr;
}

HttpRequest::Pointer MegaNz::Auth::refreshTokenRequest(std::ostream&) const {
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
