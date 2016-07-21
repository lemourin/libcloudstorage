/*****************************************************************************
 * CreateDirectoryRequest.h : CreateDirectoryRequest headers
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

#ifndef CREATEDIRECTORYREQUEST_H
#define CREATEDIRECTORYREQUEST_H

#include "Request.h"

namespace cloudstorage {

class CreateDirectoryRequest : public Request<IItem::Pointer> {
 public:
  CreateDirectoryRequest(std::shared_ptr<CloudProvider>, IItem::Pointer,
                         const std::string&, CreateDirectoryCallback);
  ~CreateDirectoryRequest();

 private:
  IItem::Pointer parent_;
  std::string name_;
  CreateDirectoryCallback callback_;
};

}  // namespace cloudstorage

#endif  // CREATEDIRECTORYREQUEST_H
