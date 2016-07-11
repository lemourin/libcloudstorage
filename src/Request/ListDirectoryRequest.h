/*****************************************************************************
 * ListDirectoryRequest.h : ListDirectoryRequest headers
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

#ifndef LISTDIRECTORYREQUEST_H
#define LISTDIRECTORYREQUEST_H

#include "IItem.h"
#include "Request.h"

namespace cloudstorage {

class ListDirectoryRequest : public Request<std::vector<IItem::Pointer>> {
 public:
  using ICallback = IListDirectoryCallback;

  ListDirectoryRequest(std::shared_ptr<CloudProvider>, IItem::Pointer directory,
                       ICallback::Pointer);
  ~ListDirectoryRequest();

 protected:
  void error(int code, const std::string& description);

 private:
  IItem::Pointer directory_;
  ICallback::Pointer callback_;
};

}  // namespace cloudstorage

#endif  // LISTDIRECTORYREQUEST_H
