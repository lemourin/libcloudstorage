/*****************************************************************************
 * Mega.cpp : Mega implementation
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

#include "Request/Request.h"
#include "Utility/Item.h"
#include "Utility/Utility.h"

#include <fstream>

using namespace mega;

const auto WAIT_INTERVAL = std::chrono::milliseconds(100);
const int BUFFER_SIZE = 1024;
const std::string MEGA_CACHE_FILE = ".mega_cache";

namespace cloudstorage {

namespace {

class Listener {
 public:
  static constexpr int IN_PROGRESS = -1;
  static constexpr int FAILURE = 0;
  static constexpr int SUCCESS = 1;

  Listener() : status_(IN_PROGRESS) {}

  std::condition_variable condition_;
  std::atomic_int status_;
};

class RequestListener : public mega::MegaRequestListener, public Listener {
 public:
  void onRequestFinish(MegaApi*, MegaRequest* r, MegaError* e) {
    if (e->getErrorCode() == 0)
      status_ = SUCCESS;
    else
      status_ = FAILURE;
    if (r->getLink()) link_ = r->getLink();
    condition_.notify_all();
  }

  std::string link_;
};

class TransferListener : public mega::MegaTransferListener, public Listener {
 public:
  bool onTransferData(MegaApi*, MegaTransfer* t, char* buffer, size_t size) {
    if (download_callback_) {
      download_callback_->receivedData(buffer, size);
      download_callback_->progress(t->getTotalBytes(),
                                   t->getTransferredBytes());
    }
    return !request_->is_cancelled();
  }

  void onTransferUpdate(MegaApi*, MegaTransfer* t) {
    if (upload_callback_)
      upload_callback_->progress(t->getTotalBytes(), t->getTransferredBytes());
  }

  void onTransferFinish(MegaApi*, MegaTransfer*, MegaError* e) {
    if (e->getErrorCode() == 0)
      status_ = SUCCESS;
    else
      status_ = FAILURE;
    condition_.notify_all();
  }

  IDownloadFileCallback::Pointer download_callback_;
  IUploadFileCallback::Pointer upload_callback_;
  Request<void>* request_;
};

}  // namespace

MegaNz::MegaNz()
    : CloudProvider(make_unique<Auth>()), mega_("ZVhB0Czb"), authorized_() {}

std::string MegaNz::name() const { return "mega"; }

IItem::Pointer MegaNz::rootDirectory() const {
  return make_unique<Item>("root", "/", IItem::FileType::Directory);
}

AuthorizeRequest::Pointer MegaNz::authorizeAsync() {
  return make_unique<Authorize>(shared_from_this(), [this](
                                                        AuthorizeRequest* r) {
    RequestListener auth_listener;
    const std::string mail = "lemourin@gmail.com";
    const char* private_key = "Co1jRlbRvWpw-u14yT_RrA";
    std::unique_ptr<char> hash(mega_.getStringHash(private_key, mail.c_str()));
    mega_.fastLogin(mail.c_str(), hash.get(), private_key, &auth_listener);
    {
      std::mutex mutex;
      std::unique_lock<std::mutex> lock(mutex);
      while (auth_listener.status_ == RequestListener::IN_PROGRESS &&
             !r->is_cancelled())
        auth_listener.condition_.wait_for(lock, WAIT_INTERVAL);
    }
    if (auth_listener.status_ != RequestListener::SUCCESS) return false;

    RequestListener fetch_nodes_listener_;
    mega_.fetchNodes(&fetch_nodes_listener_);
    {
      std::mutex mutex;
      std::unique_lock<std::mutex> lock(mutex);
      while (fetch_nodes_listener_.status_ == RequestListener::IN_PROGRESS &&
             !r->is_cancelled())
        fetch_nodes_listener_.condition_.wait_for(lock, WAIT_INTERVAL);
    }
    if (fetch_nodes_listener_.status_ != RequestListener::SUCCESS) return false;
    authorized_ = true;
    return true;
  });
}

ICloudProvider::GetItemDataRequest::Pointer MegaNz::getItemDataAsync(
    const std::string& id, GetItemDataCallback callback) {
  auto r = std::make_shared<Request<IItem::Pointer>>(shared_from_this());
  r->set_resolver(
      [id, callback, this](Request<IItem::Pointer>* r) -> IItem::Pointer {
        auto node = mega_.getNodeByPath(id.c_str());
        if (!node) {
          callback(nullptr);
          return nullptr;
        }
        auto item = toItem(node);
        RequestListener listener;
        mega_.exportNode(node, &listener);
        {
          std::mutex mutex;
          std::unique_lock<std::mutex> lock(mutex);
          while (listener.status_ == RequestListener::IN_PROGRESS &&
                 !r->is_cancelled())
            listener.condition_.wait_for(lock, WAIT_INTERVAL);
        }
        if (listener.status_ == RequestListener::SUCCESS)
          static_cast<Item*>(item.get())->set_url(listener.link_);
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
        mega_.getNodeByPath(item->id().c_str()));
    std::vector<IItem::Pointer> result;
    if (node) {
      std::unique_ptr<mega::MegaNodeList> lst(mega_.getChildren(node.get()));
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
    auto node = mega_.getNodeByPath(item->id().c_str());
    TransferListener listener;
    listener.download_callback_ = callback;
    listener.request_ = r;
    mega_.startStreaming(node, 0, node->getSize(), &listener);
    {
      std::mutex mutex;
      std::unique_lock<std::mutex> lock(mutex);
      while (listener.status_ == RequestListener::IN_PROGRESS &&
             !r->is_cancelled())
        listener.condition_.wait_for(lock, WAIT_INTERVAL);
    }
    if (listener.status_ != Listener::SUCCESS)
      callback->error("Failed to download");
    else
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
    TransferListener listener;
    listener.upload_callback_ = callback;
    listener.request_ = r;
    {
      std::fstream mega_cache(MEGA_CACHE_FILE.c_str(),
                              std::fstream::out | std::fstream::binary);
      std::array<char, BUFFER_SIZE> buffer;
      while (auto length = callback->putData(buffer.data(), BUFFER_SIZE)) {
        if (r->is_cancelled()) return;
        mega_cache.write(buffer.data(), length);
      }
    }
    mega_.startUpload(MEGA_CACHE_FILE.c_str(),
                      mega_.getNodeByPath(item->id().c_str()), filename.c_str(),
                      &listener);
    {
      std::mutex mutex;
      std::unique_lock<std::mutex> lock(mutex);
      while (listener.status_ == RequestListener::IN_PROGRESS &&
             !r->is_cancelled())
        listener.condition_.wait_for(lock, WAIT_INTERVAL);
    }
    if (listener.status_ != Listener::SUCCESS)
      callback->error("Failed to upload");
    else
      callback->done();
  });
  return r;
}

IItem::Pointer MegaNz::toItem(MegaNode* node) {
  std::unique_ptr<char> path(mega_.getNodePath(node));
  auto item = std::make_shared<Item>(
      node->getName(), path.get(),
      node->isFolder() ? IItem::FileType::Directory : IItem::FileType::Unknown);
  return item;
}

MegaNz::Auth::Auth() {}

std::string MegaNz::Auth::authorizeLibraryUrl() const { return nullptr; }

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
