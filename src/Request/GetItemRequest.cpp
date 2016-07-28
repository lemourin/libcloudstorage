/*****************************************************************************
 * GetItemRequest.cpp : GetItemRequest implementation
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

#include "GetItemRequest.h"

#include "CloudProvider/CloudProvider.h"
#include "Utility/HttpRequest.h"

namespace cloudstorage {

GetItemRequest::GetItemRequest(std::shared_ptr<CloudProvider> p,
                               const std::string& path, Callback callback)
    : Request(p), path_(path), callback_(callback) {
  set_resolver([this](Request*) -> IItem::Pointer {
    if (path_.empty() || path_.front() != '/') {
      if (callback_) callback_(nullptr);
      return nullptr;
    }
    IItem::Pointer node = provider()->rootDirectory();
    std::stringstream stream(path_.substr(1));
    std::string token;
    while (std::getline(stream, token, '/')) {
      if (!node || node->type() != IItem::FileType::Directory) {
        node = nullptr;
        break;
      }
      {
        std::lock_guard<std::mutex> lock(mutex_);
        if (is_cancelled()) return nullptr;
        current_request_ =
            provider()->listDirectoryAsync(std::move(node), nullptr);
      }
      node = getItem(current_request_->result(), token);
    }
    if (callback_) callback_(node);
    return node;
  });
  set_cancel_callback([this]() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (current_request_) current_request_->cancel();
  });
}

GetItemRequest::~GetItemRequest() { cancel(); }

IItem::Pointer GetItemRequest::getItem(const std::vector<IItem::Pointer>& items,
                                       const std::string& name) const {
  for (const IItem::Pointer& i : items)
    if (i->filename() == name) return i;
  return nullptr;
}

}  // namespace cloudstorage
