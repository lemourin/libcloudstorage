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

namespace cloudstorage {

ListDirectoryRequest::ListDirectoryRequest(
    std::shared_ptr<CloudProvider> p, IItem::Pointer directory,
    ICallback::Pointer callback, std::function<bool(int)> fault_tolerant)
    : Request(p) {
  set([=](Request::Ptr request) {
    if (directory->type() != IItem::FileType::Directory) {
      Error e{IHttpRequest::Forbidden, "trying to list non directory"};
      callback->done(e);
      request->done(e);
    } else {
      work(directory, "", callback, fault_tolerant);
    }
  });
}

ListDirectoryRequest::~ListDirectoryRequest() { cancel(); }

void ListDirectoryRequest::work(IItem::Pointer directory,
                                std::string page_token,
                                ICallback::Pointer callback,
                                std::function<bool(int)> fault_tolerant) {
  auto output_stream = std::make_shared<std::stringstream>();
  auto request = this->shared_from_this();
  sendRequest(
      [=](util::Output i) {
        return provider()->listDirectoryRequest(*directory, page_token, *i);
      },
      [=](EitherError<util::Output> e) {
        if (e.right() || fault_tolerant(e.left()->code_)) {
          std::string page_token = "";
          for (auto& t : request->provider()->listDirectoryResponse(
                   *directory, *output_stream, page_token)) {
            callback->receivedItem(t);
            result_.push_back(t);
          }
          if (!page_token.empty())
            work(directory, page_token, callback, fault_tolerant);
          else {
            callback->done(result_);
            request->done(result_);
          }
        } else {
          callback->done(e.left());
          request->done(e.left());
        }
      },
      output_stream);
}

}  // namespace cloudstorage
