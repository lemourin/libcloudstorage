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

#ifdef WITH_MEGA

#include "MegaNz.h"

#include "IAuth.h"
#include "Request/DownloadFileRequest.h"
#include "Request/Request.h"
#include "Utility/FileServer.h"
#include "Utility/Item.h"
#include "Utility/Utility.h"

#include <json/json.h>
#include <array>
#include <condition_variable>
#include <cstring>
#include <fstream>
#include <queue>

using namespace mega;
using namespace std::placeholders;

const int BUFFER_SIZE = 1024;
const int CACHE_FILENAME_LENGTH = 12;

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
  static constexpr int PAUSED = 3;

  template <class T>
  static std::shared_ptr<T> make(Callback cb, MegaNz* provider) {
    auto r = std::make_shared<T>(cb, provider);
    provider->addRequestListener(r);
    return r;
  }

  Listener(Callback cb, MegaNz* provider)
      : status_(IN_PROGRESS),
        error_({IHttpRequest::Unknown, ""}),
        callback_(cb),
        provider_(provider) {}

  ~Listener() override { cancel(); }

  void cancel() override {
    std::unique_lock<std::recursive_mutex> lock(mutex_);
    if (status_ != IN_PROGRESS) return;
    status_ = CANCELLED;
    error_ = {IHttpRequest::Aborted, util::Error::ABORTED};
    auto callback = util::exchange(callback_, nullptr);
    lock.unlock();
    if (callback) callback(error_, this);
    finish();
  }

  EitherError<void> result() override {
    finish();
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (status_ != SUCCESS)
      return error_;
    else
      return nullptr;
  }

  void finish() override {
    std::unique_lock<std::recursive_mutex> lock(mutex_);
    condition_.wait(lock, [this]() { return status_ != IN_PROGRESS; });
  }

  void pause() override {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (status_ != CANCELLED) status_ = PAUSED;
  }

  void resume() override {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (status_ == PAUSED) status_ = IN_PROGRESS;
  }

  int status() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return status_;
  }

 protected:
  int status_;
  std::recursive_mutex mutex_;
  std::condition_variable_any condition_;
  Error error_;
  Callback callback_;
  MegaNz* provider_;
};

class RequestListener : public mega::MegaRequestListener, public Listener {
 public:
  using Listener::Listener;

  void onRequestFinish(MegaApi*, MegaRequest* r, MegaError* e) override {
    auto p = shared_from_this();
    provider_->removeRequestListener(p);
    std::unique_lock<std::recursive_mutex> lock(mutex_);
    if (e->getErrorCode() == 0)
      status_ = SUCCESS;
    else {
      status_ = FAILURE;
      error_ = {e->getErrorCode(), e->getErrorString()};
    }
    if (r->getLink()) link_ = r->getLink();
    if (r->getMegaAccountDetails()) {
      space_total_ = r->getMegaAccountDetails()->getStorageMax();
      space_used_ = r->getMegaAccountDetails()->getStorageUsed();
    }
    node_ = r->getNodeHandle();
    auto callback = util::exchange(callback_, nullptr);
    lock.unlock();
    if (callback) {
      if (e->getErrorCode() == 0)
        callback(nullptr, this);
      else
        callback(error_, this);
    }
    condition_.notify_all();
  }

  uint64_t space_total_ = 0;
  uint64_t space_used_ = 0;
  std::string link_;
  MegaHandle node_ = 0;
};

class TransferListener : public mega::MegaTransferListener, public Listener {
 public:
  using Listener::Listener;

  void cancel() override {
    std::unique_lock<std::mutex> lock(callback_mutex_);
    auto mega = util::exchange(mega_, nullptr);
    auto transfer = util::exchange(transfer_, 0);
    upload_callback_ = nullptr;
    download_callback_ = nullptr;
    lock.unlock();
    if (mega && transfer) mega->cancelTransferByTag(transfer);
    Listener::cancel();
  }

  void onTransferStart(MegaApi* mega, MegaTransfer* transfer) override {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
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
    std::unique_lock<std::mutex> lock(callback_mutex_);
    if (download_callback_) {
      download_callback_->receivedData(buffer, size);
      download_callback_->progress(t->getTotalBytes(),
                                   t->getTransferredBytes());
    }
    return true;
  }

  void onTransferUpdate(MegaApi* mega, MegaTransfer* t) override {
    if (status() == CANCELLED) return mega->cancelTransfer(t);
    std::unique_lock<std::mutex> lock(callback_mutex_);
    if (upload_callback_)
      upload_callback_->progress(t->getTotalBytes(), t->getTransferredBytes());
  }

  void onTransferFinish(MegaApi*, MegaTransfer* t, MegaError* e) override {
    auto r = shared_from_this();
    provider_->removeRequestListener(r);
    std::unique_lock<std::recursive_mutex> lock(mutex_);
    if (e->getErrorCode() == 0) {
      status_ = SUCCESS;
      node_ = t->getNodeHandle();
    } else {
      error_ = {e->getErrorCode(), e->getErrorString()};
      status_ = FAILURE;
    }
    auto callback = util::exchange(callback_, nullptr);
    lock.unlock();
    if (callback) {
      if (e->getErrorCode() == 0)
        callback(nullptr, this);
      else
        callback(error_, this);
    }
    condition_.notify_all();
  }

  IDownloadFileCallback* download_callback_ = nullptr;
  IUploadFileCallback* upload_callback_ = nullptr;
  MegaApi* mega_ = nullptr;
  int transfer_ = 0;
  MegaHandle node_ = 0;
  std::shared_ptr<RequestListener> listener_;
  std::mutex callback_mutex_;
};

}  // namespace

MegaNz::MegaNz()
    : CloudProvider(util::make_unique<Auth>()),
      mega_(),
      authorized_(),
      engine_(device_()),
      temporary_directory_(".") {}

MegaNz::~MegaNz() { mega_ = nullptr; }

void MegaNz::addRequestListener(
    std::shared_ptr<IRequest<EitherError<void>>> p) {
  std::lock_guard<std::mutex> lock(mutex_);
  request_listeners_.insert(p);
}

void MegaNz::removeRequestListener(
    std::shared_ptr<IRequest<EitherError<void>>> p) {
  std::lock_guard<std::mutex> lock(mutex_);
  request_listeners_.erase(request_listeners_.find(p));
}

std::unique_ptr<mega::MegaNode> MegaNz::node(const std::string& id) const {
  if (id == rootDirectory()->id())
    return std::unique_ptr<MegaNode>(mega_->getRootNode());
  else
    return std::unique_ptr<MegaNode>(mega_->getNodeByHandle(std::stoull(id)));
}

template <class T>
std::shared_ptr<IRequest<EitherError<void>>> MegaNz::make_request(
    std::function<void(T*)> init, Listener::Callback c,
    GenericCallback<EitherError<void>>) {
  auto r = Listener::make<T>(c, this);
  init(r.get());
  return r;
}

template <class T>
void MegaNz::make_subrequest(
    Request<EitherError<void>>::Pointer parent_request,
    std::function<void(T*)> init,
    std::function<void(EitherError<void>, Listener*)> listener_callback) {}

void MegaNz::initialize(InitData&& data) {
  {
    auto lock = auth_lock();
    setWithHint(data.hints_, "temporary_directory",
                [this](std::string v) { temporary_directory_ = v; });
    setWithHint(data.hints_, "client_id", [this](std::string v) {
      mega_ =
          util::make_unique<MegaApi>(v.c_str(), temporary_directory_.c_str());
    });
    if (!mega_)
      mega_ =
          util::make_unique<MegaApi>("ZVhB0Czb", temporary_directory_.c_str());
  }
  CloudProvider::initialize(std::move(data));
}

std::string MegaNz::name() const { return "mega"; }

std::string MegaNz::endpoint() const { return file_url(); }

ICloudProvider::Hints MegaNz::hints() const {
  Hints result = {{"temporary_directory", temporary_directory_}};
  auto t = CloudProvider::hints();
  result.insert(t.begin(), t.end());
  return result;
}

ICloudProvider::ExchangeCodeRequest::Pointer MegaNz::exchangeCodeAsync(
    const std::string& code, ExchangeCodeCallback callback) {
  return std::make_shared<Request<EitherError<Token>>>(
             shared_from_this(), callback,
             [=](Request<EitherError<Token>>::Pointer r) {
               auto token = authorizationCodeToToken(code);
               auto ret = token->token_.empty()
                              ? EitherError<Token>(Error{
                                    IHttpRequest::Failure,
                                    util::Error::INVALID_AUTHORIZATION_CODE})
                              : EitherError<Token>({token->token_, ""});
               r->done(ret);
             })
      ->run();
}

AuthorizeRequest::Pointer MegaNz::authorizeAsync() {
  return std::make_shared<AuthorizeRequest>(
      shared_from_this(), [=](AuthorizeRequest::Pointer r,
                              AuthorizeRequest::AuthorizeCompleted complete) {
        auto fetch = [=]() {
          r->make_subrequest<MegaNz>(
              &MegaNz::make_request<RequestListener>,
              [=](RequestListener* r) { mega_->fetchNodes(r); },
              [=](EitherError<void> e, Listener*) {
                if (!e.left()) authorized_ = true;
                complete(e);
              },
              complete);
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
            complete(Error{IHttpRequest::Unauthorized,
                           util::Error::INVALID_CREDENTIALS});
          }
        });
      });
}

ICloudProvider::GetItemDataRequest::Pointer MegaNz::getItemDataAsync(
    const std::string& id, GetItemDataCallback callback) {
  return std::make_shared<Request<EitherError<IItem>>>(
             shared_from_this(), callback,
             [=](Request<EitherError<IItem>>::Pointer r) {
               ensureAuthorized<EitherError<IItem>>(r, [=] {
                 auto node = this->node(id);
                 if (!node)
                   return r->done(Error{IHttpRequest::NotFound, "not found"});
                 return r->done(toItem(node.get()));
               });
             })
      ->run();
}

ICloudProvider::DownloadFileRequest::Pointer MegaNz::downloadFileAsync(
    IItem::Pointer item, IDownloadFileCallback::Pointer callback, Range range) {
  return std::make_shared<Request<EitherError<void>>>(
             shared_from_this(),
             [=](EitherError<void> e) { callback->done(e); },
             downloadResolver(item, callback.get(), range))
      ->run();
}

ICloudProvider::UploadFileRequest::Pointer MegaNz::uploadFileAsync(
    IItem::Pointer item, const std::string& filename,
    IUploadFileCallback::Pointer cb) {
  auto callback = cb.get();
  auto resolver = [=](Request<EitherError<IItem>>::Pointer r) {
    thread_pool()->schedule([=] {
      ensureAuthorized<EitherError<IItem>>(r, [=] {
        std::string cache = temporaryFileName();
        {
          std::fstream mega_cache(cache.c_str(),
                                  std::fstream::out | std::fstream::binary);
          if (!mega_cache)
            return r->done(Error{IHttpRequest::Forbidden,
                                 util::Error::COULD_NOT_READ_FILE});
          std::array<char, BUFFER_SIZE> buffer;
          while (auto length = callback->putData(buffer.data(), BUFFER_SIZE)) {
            if (r->is_cancelled()) {
              (void)std::remove(cache.c_str());
              return r->done(
                  Error{IHttpRequest::Aborted, util::Error::ABORTED});
            }
            mega_cache.write(buffer.data(), length);
          }
        }
        auto node = this->node(item->id());
        if (!node)
          return r->done(
              Error{IHttpRequest::NotFound, util::Error::NODE_NOT_FOUND});
        auto node_ptr = node.get();
        r->make_subrequest<MegaNz>(
            &MegaNz::make_request<TransferListener>,
            [=](TransferListener* r) {
              r->upload_callback_ = callback;
              mega_->startUpload(cache.c_str(), node_ptr, filename.c_str(), r);
            },
            [=](EitherError<void> e, Listener* listener) {
              (void)std::remove(cache.c_str());
              if (e.left()) return r->done(e.left());
              std::unique_ptr<MegaNode> node(mega_->getNodeByHandle(
                  static_cast<TransferListener*>(listener)->node_));
              r->done(toItem(node.get()));
            },
            [=](EitherError<void> e) { r->done(e.left()); });
      });
    });
  };
  return std::make_shared<Request<EitherError<IItem>>>(
             shared_from_this(), [=](EitherError<IItem> e) { cb->done(e); },
             resolver)
      ->run();
}

ICloudProvider::DownloadFileRequest::Pointer MegaNz::getThumbnailAsync(
    IItem::Pointer item, IDownloadFileCallback::Pointer callback) {
  auto resolver = [=](Request<EitherError<void>>::Pointer r) {
    thread_pool()->schedule([=] {
      ensureAuthorized<EitherError<void>>(r, [=] {
        auto node = this->node(item->id());
        if (!node)
          return r->done(
              Error{IHttpRequest::NotFound, util::Error::NODE_NOT_FOUND});
        std::string cache = temporaryFileName();
        auto node_ptr = node.get();
        r->make_subrequest<MegaNz>(
            &MegaNz::make_request<RequestListener>,
            [=](RequestListener* r) {
              mega_->getThumbnail(node_ptr, cache.c_str(), r);
            },
            [=](EitherError<void> e, Listener*) {
              if (e.left()) return r->done(e.left());
              std::fstream cache_file(cache.c_str(),
                                      std::fstream::in | std::fstream::binary);
              if (!cache_file)
                return r->done(Error{IHttpRequest::Failure,
                                     util::Error::COULD_NOT_READ_FILE});
              std::array<char, BUFFER_SIZE> buffer;
              do {
                cache_file.read(buffer.data(), BUFFER_SIZE);
                callback->receivedData(buffer.data(), cache_file.gcount());
              } while (cache_file.gcount() > 0);
              (void)std::remove(cache.c_str());
              r->done(nullptr);
            },
            [=](EitherError<void> e) { r->done(e.left()); });
      });
    });
  };
  return std::make_shared<Request<EitherError<void>>>(
             shared_from_this(),
             [=](EitherError<void> e) { callback->done(e); }, resolver)
      ->run();
}

ICloudProvider::DeleteItemRequest::Pointer MegaNz::deleteItemAsync(
    IItem::Pointer item, DeleteItemCallback callback) {
  auto resolver = [=](Request<EitherError<void>>::Pointer r) {
    ensureAuthorized<EitherError<void>>(r, [=] {
      auto node = this->node(item->id());
      if (!node) {
        r->done(Error{IHttpRequest::NotFound, util::Error::NODE_NOT_FOUND});
      } else {
        auto node_ptr = node.get();
        r->make_subrequest<MegaNz>(
            &MegaNz::make_request<RequestListener>,
            [=](RequestListener* r) { mega_->remove(node_ptr, r); },
            [=](EitherError<void> e, Listener*) { r->done(e); },
            [=](EitherError<void> e) { r->done(e); });
      }
    });
  };
  return std::make_shared<Request<EitherError<void>>>(shared_from_this(),
                                                      callback, resolver)
      ->run();
}

ICloudProvider::CreateDirectoryRequest::Pointer MegaNz::createDirectoryAsync(
    IItem::Pointer parent, const std::string& name,
    CreateDirectoryCallback callback) {
  auto resolver = [=](Request<EitherError<IItem>>::Pointer r) {
    ensureAuthorized<EitherError<IItem>>(r, [=] {
      auto parent_node = this->node(parent->id());
      if (!parent_node)
        return r->done(
            Error{IHttpRequest::NotFound, util::Error::NODE_NOT_FOUND});
      auto parent_node_ptr = parent_node.get();
      r->make_subrequest<MegaNz>(
          &MegaNz::make_request<RequestListener>,
          [=](RequestListener* r) {
            mega_->createFolder(name.c_str(), parent_node_ptr, r);
          },
          [=](EitherError<void> e, Listener* listener) {
            if (e.left()) return r->done(e.left());
            std::unique_ptr<mega::MegaNode> node(mega_->getNodeByHandle(
                static_cast<RequestListener*>(listener)->node_));
            r->done(toItem(node.get()));
          },
          [=](EitherError<void> e) { r->done(e.left()); });
    });
  };
  return std::make_shared<Request<EitherError<IItem>>>(shared_from_this(),
                                                       callback, resolver)
      ->run();
}

ICloudProvider::MoveItemRequest::Pointer MegaNz::moveItemAsync(
    IItem::Pointer source, IItem::Pointer destination,
    MoveItemCallback callback) {
  auto resolver = [=](Request<EitherError<IItem>>::Pointer r) {
    ensureAuthorized<EitherError<IItem>>(r, [=] {
      auto source_node = this->node(source->id());
      auto destination_node = this->node(destination->id());
      if (source_node && destination_node) {
        auto source_node_ptr = source_node.get();
        auto destination_node_ptr = destination_node.get();
        r->make_subrequest<MegaNz>(
            &MegaNz::make_request<RequestListener>,
            [=](RequestListener* r) {
              mega_->moveNode(source_node_ptr, destination_node_ptr, r);
            },
            [=](EitherError<void> e, Listener* listener) {
              if (e.left()) return r->done(e.left());
              std::unique_ptr<mega::MegaNode> node(mega_->getNodeByHandle(
                  static_cast<RequestListener*>(listener)->node_));
              r->done(toItem(node.get()));
            },
            [=](EitherError<void> e) { r->done(e.left()); });
      } else {
        r->done(Error{IHttpRequest::NotFound, util::Error::NODE_NOT_FOUND});
      }
    });
  };
  return std::make_shared<Request<EitherError<IItem>>>(shared_from_this(),
                                                       callback, resolver)
      ->run();
}

ICloudProvider::RenameItemRequest::Pointer MegaNz::renameItemAsync(
    IItem::Pointer item, const std::string& name, RenameItemCallback callback) {
  auto resolver = [=](Request<EitherError<IItem>>::Pointer r) {
    ensureAuthorized<EitherError<IItem>>(r, [=] {
      auto node = this->node(item->id());
      if (node) {
        auto node_ptr = node.get();
        r->make_subrequest<MegaNz>(
            &MegaNz::make_request<RequestListener>,
            [=](RequestListener* r) {
              mega_->renameNode(node_ptr, name.c_str(), r);
            },
            [=](EitherError<void> e, Listener* listener) {
              if (e.left()) return r->done(e.left());
              std::unique_ptr<mega::MegaNode> node(mega_->getNodeByHandle(
                  static_cast<RequestListener*>(listener)->node_));
              r->done(toItem(node.get()));
            },
            [=](EitherError<void> e) { r->done(e.left()); });
      } else
        r->done(Error{IHttpRequest::NotFound, util::Error::NODE_NOT_FOUND});
    });
  };
  return std::make_shared<Request<EitherError<IItem>>>(shared_from_this(),
                                                       callback, resolver)
      ->run();
}

ICloudProvider::ListDirectoryPageRequest::Pointer
MegaNz::listDirectoryPageAsync(IItem::Pointer item, const std::string&,
                               ListDirectoryPageCallback complete) {
  auto resolver = [=](Request<EitherError<PageData>>::Pointer r) {
    ensureAuthorized<EitherError<PageData>>(r, [=] {
      auto node = this->node(item->id());
      if (node) {
        IItem::List result;
        std::unique_ptr<mega::MegaNodeList> lst(mega_->getChildren(node.get()));
        if (lst) {
          for (int i = 0; i < lst->size(); i++) {
            auto item = toItem(lst->get(i));
            result.push_back(item);
          }
        }
        r->done(PageData{result, ""});
      } else
        r->done(Error{IHttpRequest::NotFound, util::Error::NODE_NOT_FOUND});
    });
  };
  return std::make_shared<Request<EitherError<PageData>>>(shared_from_this(),
                                                          complete, resolver)
      ->run();
}

ICloudProvider::GeneralDataRequest::Pointer MegaNz::getGeneralDataAsync(
    GeneralDataCallback callback) {
  auto resolver = [=](Request<EitherError<GeneralData>>::Pointer r) {
    ensureAuthorized<EitherError<GeneralData>>(r, [=] {
      r->make_subrequest<MegaNz>(
          &MegaNz::make_request<RequestListener>,
          [=](RequestListener* r) { mega()->getAccountDetails(r); },
          [=](EitherError<void> e, Listener* listener) {
            if (e.left()) return r->done(e.left());
            GeneralData result;
            result.username_ =
                credentialsFromString(token())["username"].asString();
            result.space_total_ =
                static_cast<RequestListener*>(listener)->space_total_;
            result.space_used_ =
                static_cast<RequestListener*>(listener)->space_used_;
            r->done(result);
          },
          [=](EitherError<void> e) { r->done(e.left()); });
    });
  };
  return std::make_shared<Request<EitherError<GeneralData>>>(shared_from_this(),
                                                             callback, resolver)
      ->run();
}

std::function<void(Request<EitherError<void>>::Pointer)>
MegaNz::downloadResolver(IItem::Pointer item, IDownloadFileCallback* callback,
                         Range range) {
  return [=](Request<EitherError<void>>::Pointer r) {
    ensureAuthorized<EitherError<void>>(r, [=] {
      auto node = this->node(item->id());
      if (!node)
        return r->done(
            Error{IHttpRequest::NotFound, util::Error::NODE_NOT_FOUND});
      auto node_ptr = node.get();
      r->make_subrequest<MegaNz>(
          &MegaNz::make_request<TransferListener>,
          [=](TransferListener* r) {
            r->download_callback_ = callback;
            mega_->startStreaming(
                node_ptr, range.start_,
                range.size_ == Range::Full
                    ? static_cast<uint64_t>(node_ptr->getSize()) - range.start_
                    : range.size_,
                r);
          },
          [=](EitherError<void> e, Listener*) { r->done(e); },
          [=](EitherError<void> e) { r->done(e.left()); });
    });
  };
}

void MegaNz::login(Request<EitherError<void>>::Pointer r,
                   AuthorizeRequest::AuthorizeCompleted complete) {
  auto session_auth_listener_callback = [=](EitherError<void> e, Listener*) {
    if (e.left()) {
      r->make_subrequest<MegaNz>(
          &MegaNz::make_request<RequestListener>,
          [=](RequestListener* r) {
            auto data = credentialsFromString(token());
            std::string mail = data["username"].asString();
            std::string private_key = data["password"].asString();
            std::unique_ptr<char[]> hash(
                mega_->getStringHash(private_key.c_str(), mail.c_str()));
            mega_->fastLogin(mail.c_str(), hash.get(), private_key.c_str(), r);
          },
          [=](EitherError<void> e, Listener*) {
            if (!e.left()) {
              auto lock = auth_lock();
              std::unique_ptr<char[]> session(mega_->dumpSession());
              auth()->access_token()->token_ = session.get();
            }
            complete(e);
          },
          [=](EitherError<void> e) { complete(e.left()); });
    } else
      complete(e);
  };
  r->make_subrequest<MegaNz>(
      &MegaNz::make_request<RequestListener>,
      [=](RequestListener* r) { mega_->fastLogin(access_token().c_str(), r); },
      session_auth_listener_callback,
      [=](EitherError<void> e) { r->done(e.left()); });
}

std::string MegaNz::passwordHash(const std::string& password) const {
  std::unique_ptr<char[]> hash(mega_->getBase64PwKey(password.c_str()));
  return std::string(hash.get());
}

IItem::Pointer MegaNz::toItem(MegaNode* node) {
  auto item = util::make_unique<Item>(
      node->getName(), std::to_string(node->getHandle()),
      node->isFolder() ? IItem::UnknownSize : node->getSize(),
      node->isFolder() ? IItem::UnknownTimeStamp
                       : std::chrono::system_clock::time_point(
                             std::chrono::seconds(node->getModificationTime())),
      node->isFolder() ? IItem::FileType::Directory : IItem::FileType::Unknown);
  item->set_url(endpoint() +
                "/?id=" + util::to_base64(std::to_string(node->getHandle())) +
                "&name=" + util::Url::escape(util::to_base64(node->getName())) +
                "&size=" + std::to_string(node->getSize()) +
                "&state=" + util::Url::escape(auth()->state()));
  return std::move(item);
}

std::string MegaNz::randomString(int length) {
  std::unique_lock<std::mutex> lock(mutex_);
  std::uniform_int_distribution<short> dist('a', 'z');
  std::string result;
  for (int i = 0; i < length; i++) result += dist(engine_);
  return result;
}

std::string MegaNz::temporaryFileName() {
  return temporary_directory_ + randomString(CACHE_FILENAME_LENGTH);
}

template <class T>
void MegaNz::ensureAuthorized(typename Request<T>::Pointer r,
                              std::function<void()> on_success) {
  auto f = [=](EitherError<void> e) {
    if (e.left())
      r->done(e.left());
    else
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
  Json::Value json;
  json["username"] = data["username"].asString();
  json["password"] = passwordHash(data["password"].asString());
  token->token_ = credentialsToString(json);
  token->refresh_token_ = token->token_;
  return token;
}

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

#endif  // WITH_MEGA
