/*****************************************************************************
 * ListDirectoryRequest.cpp : ListDirectoryRequest implementation
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

#include "ListDirectoryRequest.h"

#include "CloudProvider/CloudProvider.h"

using namespace std::placeholders;

namespace cloudstorage {

ListDirectoryRequest::ListDirectoryRequest(std::shared_ptr<CloudProvider> p,
                                           const IItem::Pointer& directory,
                                           const ICallback::Pointer& cb)
    : Request(std::move(p), [=](EitherError<IItem::List> e) { cb->done(e); },
              std::bind(&ListDirectoryRequest::resolve, this, _1, directory,
                        cb.get())) {}

ListDirectoryRequest::~ListDirectoryRequest() { cancel(); }

void ListDirectoryRequest::resolve(const Request::Pointer& request,
                                   const IItem::Pointer& directory,
                                   ICallback* callback) {
  if (directory->type() != IItem::FileType::Directory)
    request->done(Error{IHttpRequest::Forbidden, util::Error::NOT_A_DIRECTORY});
  else
    work(directory, "", callback);
}

void ListDirectoryRequest::work(const IItem::Pointer& directory,
                                std::string page_token, ICallback* callback) {
  auto request = this->shared_from_this();
  request->make_subrequest(
      &CloudProvider::listDirectoryPageAsync, directory, std::move(page_token),
      [=, this](EitherError<PageData> e) {
        if (e.left()) return request->done(e.left());
        for (auto& t : e.right()->items_) {
          callback->receivedItem(t);
          result_.push_back(t);
        }
        if (!e.right()->next_token_.empty())
          work(directory, std::move(e.right()->next_token_), callback);
        else
          request->done(result_);
      });
}

}  // namespace cloudstorage
