/*****************************************************************************
 * GetItemDataRequest.h : GetItemDataRequest headers
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

#ifndef GETITEMDATAREQUEST_H
#define GETITEMDATAREQUEST_H

#include "IItem.h"
#include "ListDirectoryRequest.h"
#include "Request.h"

namespace cloudstorage {

class GetItemDataRequest : public Request {
 public:
  using Pointer = std::unique_ptr<GetItemDataRequest>;
  using Callback = std::function<void(IItem::Pointer)>;
  using Factory = std::function<IItem::Pointer(GetItemDataRequest*)>;

  GetItemDataRequest(std::shared_ptr<CloudProvider>, const std::string& id,
                     Callback, Factory = nullptr);
  ~GetItemDataRequest();

  Callback callback() const;

  void finish();
  virtual IItem::Pointer result();

 protected:
  static IItem::Pointer resolve(GetItemDataRequest*);

 private:
  std::string id_;
  Callback callback_;
  std::shared_future<IItem::Pointer> function_;
};

}  // namespace cloudstorage

#endif  // GETITEMDATAREQUEST_H
