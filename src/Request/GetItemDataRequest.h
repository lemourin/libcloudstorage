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

#include "Request.h"

namespace cloudstorage {

class GetItemDataRequest : public Request {
 public:
  using Pointer = std::unique_ptr<GetItemDataRequest>;

  GetItemDataRequest(std::shared_ptr<CloudProvider>,
                     std::function<void(IItem::Pointer)>);

  std::function<void(IItem::Pointer)> callback() const;

  virtual IItem::Pointer result() = 0;

 private:
  std::function<void(IItem::Pointer)> callback_;
};

}  // namespace cloudstorage

#endif  // GETITEMDATAREQUEST_H
