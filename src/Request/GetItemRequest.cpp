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
    : Request(p) {
  set([=](Request::Pointer) {
    if (path.empty() || path.front() != '/') {
      Error e{IHttpRequest::Forbidden, "invalid path"};
      callback(e);
      return done(e);
    }
    work(provider()->rootDirectory(), path, callback);
  });
}

GetItemRequest::~GetItemRequest() { cancel(); }

IItem::Pointer GetItemRequest::getItem(const std::vector<IItem::Pointer>& items,
                                       const std::string& name) const {
  for (const IItem::Pointer& i : items)
    if (i->filename() == name) return i;
  return nullptr;
}

void GetItemRequest::work(IItem::Pointer item, std::string p,
                          Callback complete) {
  if (!item) {
    Error e{IHttpRequest::NotFound, "not found"};
    complete(e);
    return done(e);
  }
  if (p.empty() || p.size() == 1) {
    complete(item);
    return done(item);
  }
  auto path = p.substr(1);
  auto it = path.find_first_of('/');
  std::string name = it == std::string::npos
                         ? path
                         : std::string(path.begin(), path.begin() + it),
              rest = it == std::string::npos
                         ? ""
                         : std::string(path.begin() + it, path.end());
  auto request = this->shared_from_this();
  subrequest(provider()->listDirectoryAsync(
      item, [=](EitherError<std::vector<IItem::Pointer>> e) {
        if (e.left()) {
          complete(e.left());
          request->done(e.left());
        } else {
          work(getItem(*e.right(), name), rest, complete);
        }
      }));
}

}  // namespace cloudstorage
