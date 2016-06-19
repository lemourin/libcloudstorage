/*****************************************************************************
 * Request.h : Request prototypes
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

#ifndef REQUEST_H
#define REQUEST_H

#include <future>
#include <sstream>
#include <vector>

#include "IItem.h"

namespace cloudstorage {

class CloudProvider;

class Request {
 public:
  Request(CloudProvider*);

  void cancel();

  CloudProvider* provider() const { return provider_; }
  std::iostream& input_stream() { return input_stream_; }

 private:
  CloudProvider* provider_;
  std::stringstream input_stream_;
};

class ListDirectoryRequest : public Request {
 public:
  using Pointer = std::unique_ptr<ListDirectoryRequest>;

  class ICallback {
   public:
    using Pointer = std::unique_ptr<ICallback>;

    virtual ~ICallback() = default;

    virtual void receivedItem(IItem::Pointer item) = 0;
  };

  ListDirectoryRequest(CloudProvider*, IItem::Pointer directory,
                       ICallback::Pointer);

  std::vector<IItem::Pointer> result();

 private:
  std::future<std::vector<IItem::Pointer>> result_;
  IItem::Pointer directory_;
  ICallback::Pointer callback_;
};

class GetItemRequest : public Request {
 public:
  using Pointer = std::unique_ptr<GetItemRequest>;

  GetItemRequest(CloudProvider*, const std::string& path,
                 std::function<void(IItem::Pointer)> callback);

  IItem::Pointer result();

 private:
  IItem::Pointer getItem(std::vector<IItem::Pointer>&& items,
                         const std::string& name) const;

  std::future<IItem::Pointer> result_;
  std::string path_;
  std::function<void(IItem::Pointer)> callback_;
};

}  // namespace cloudstorage

#endif  // REQUEST_H
