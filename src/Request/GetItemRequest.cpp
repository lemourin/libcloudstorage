/*****************************************************************************
 * GetItemRequest.cpp : GetItemRequest implementation
 *
 *****************************************************************************
 * Copyright (C) 2016-2016 VideoLAN
 *
 * Authors: Paweł Wegner <pawel.wegner95@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "GetItemRequest.h"

#include "CloudProvider.h"
#include "HttpCallback.h"
#include "HttpRequest.h"

namespace cloudstorage {

GetItemRequest::GetItemRequest(std::shared_ptr<CloudProvider> p,
                               const std::string& path,
                               std::function<void(IItem::Pointer)> callback)
    : Request(p), path_(path), callback_(callback) {
  result_ = std::async(std::launch::async, [this]() -> IItem::Pointer {
    if (path_.empty() || path_.front() != '/') {
      if (callback_) callback_(nullptr);
      return nullptr;
    }
    IItem::Pointer node = provider()->rootDirectory();
    std::stringstream stream(path_.substr(1));
    std::string token;
    while (std::getline(stream, token, '/')) {
      if (!node || !node->is_directory()) {
        node = nullptr;
        break;
      }
      std::shared_future<std::vector<IItem::Pointer>> current_directory;
      {
        std::lock_guard<std::mutex> lock(mutex_);
        if (is_cancelled()) return nullptr;
        current_request_ =
            provider()->listDirectoryAsync(std::move(node), nullptr);
        current_directory = current_request_->result_;
      }
      node = getItem(current_directory.get(), token);
    }
    if (callback_) callback_(node);
    return node;
  });
}

GetItemRequest::~GetItemRequest() { cancel(); }

void GetItemRequest::finish() {
  if (result_.valid()) result_.get();
}

void GetItemRequest::cancel() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    set_cancelled(true);
    if (current_request_) current_request_->cancel();
  }
  Request::cancel();
}

IItem::Pointer GetItemRequest::result() {
  std::shared_future<IItem::Pointer> future = result_;
  if (!future.valid()) throw std::logic_error("Future invalid.");
  return future.get();
}

IItem::Pointer GetItemRequest::getItem(const std::vector<IItem::Pointer>& items,
                                       const std::string& name) const {
  for (const IItem::Pointer& i : items)
    if (i->filename() == name) return i;
  return nullptr;
}

}  // namespace cloudstorage
