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

namespace cloudstorage {

GetItemRequest::GetItemRequest(std::shared_ptr<CloudProvider> p,
                               const std::string& path, Callback callback)
    : Request(p), path_(path), callback_(callback) {
  set_resolver([this](Request*) -> EitherError<IItem> {
    if (path_.empty() || path_.front() != '/') {
      Error e{IHttpRequest::Forbidden, "invalid path"};
      callback_(e);
      return e;
    }
    IItem::Pointer node = provider()->rootDirectory();
    std::stringstream stream(path_.substr(1));
    std::string token;
    while (std::getline(stream, token, '/')) {
      if (!node || node->type() != IItem::FileType::Directory) {
        node = nullptr;
        break;
      }
      auto current_request = static_cast<ICloudProvider*>(provider().get())
                                 ->listDirectoryAsync(std::move(node));
      subrequest(current_request);
      if (current_request->result().left()) {
        callback_(current_request->result().left());
        return current_request->result().left();
      }
      node = getItem(*current_request->result().right(), token);
    }
    callback_(node);
    return node;
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
