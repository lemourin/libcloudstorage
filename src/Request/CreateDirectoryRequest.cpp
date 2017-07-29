/*****************************************************************************
 * CreateDirectoryRequest.cpp : CreateDirectoryRequest implementation
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

#include "CreateDirectoryRequest.h"

#include "CloudProvider/CloudProvider.h"

namespace cloudstorage {

CreateDirectoryRequest::CreateDirectoryRequest(std::shared_ptr<CloudProvider> p,
                                               IItem::Pointer parent,
                                               const std::string& name,
                                               CreateDirectoryCallback callback)
    : Request(p), parent_(parent), name_(name), callback_(callback) {
  set_resolver([this](Request* r) -> EitherError<IItem> {
    if (parent_->type() != IItem::FileType::Directory) {
      Error e{403, "parent not a directory"};
      callback_(e);
      return e;
    }
    std::stringstream output;
    Error error;
    int code = r->sendRequest(
        [this](std::ostream& stream) {
          return provider()->createDirectoryRequest(*parent_, name_, stream);
        },
        output, &error);
    if (IHttpRequest::isSuccess(code)) {
      auto i = provider()->createDirectoryResponse(output);
      callback_(i);
      return i;
    } else {
      callback_(error);
      return error;
    }
  });
}

CreateDirectoryRequest::~CreateDirectoryRequest() { cancel(); }

}  // namespace cloudstorage
