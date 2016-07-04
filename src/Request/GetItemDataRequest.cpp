/*****************************************************************************
 * GetItemDataRequest.cpp : GetItemDataRequest implementation
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

#include "GetItemDataRequest.h"

#include "CloudProvider/CloudProvider.h"

namespace cloudstorage {

GetItemDataRequest::GetItemDataRequest(
    std::shared_ptr<CloudProvider> p, IItem::Pointer item,
    std::function<void(IItem::Pointer)> callback, bool ready)
    : Request(p), item_(item), callback_(callback) {
  if (ready) callback(item);
}

IItem::Pointer GetItemDataRequest::item() const { return item_; }

std::function<void(IItem::Pointer)> GetItemDataRequest::callback() const {
  return callback_;
}

void GetItemDataRequest::finish() {}

IItem::Pointer GetItemDataRequest::result() { return item_; }

}  // namespace cloudstorage
