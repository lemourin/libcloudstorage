/*****************************************************************************
 * ListDirectoryPageRequest.cpp
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

#include "ListDirectoryPageRequest.h"

#include "CloudProvider/CloudProvider.h"

namespace cloudstorage {

ListDirectoryPageRequest::ListDirectoryPageRequest(
    std::shared_ptr<CloudProvider> p, IItem::Pointer directory,
    const std::string& token, ListDirectoryPageCallback completed,
    std::function<bool(int)> fault_tolerant)
    : Request(p) {
  set([=](Request<EitherError<PageData>>::Ptr r) {
    if (directory->type() != IItem::FileType::Directory) {
      Error e{IHttpRequest::Bad, "file not a directory"};
      completed(e);
      return r->done(e);
    }
    auto output = std::make_shared<std::stringstream>();
    r->sendRequest(
        [=](util::Output) {
          return r->provider()->listDirectoryRequest(*directory, token,
                                                     *output);
        },
        [=](EitherError<util::Output> e) {
          if (e.left()) {
            if (!fault_tolerant(e.left()->code_)) {
              completed(e.left());
              return r->done(e.left());
            } else {
              completed(PageData{{}, ""});
              return r->done(PageData{{}, ""});
            }
          }
          std::string next_token;
          auto lst = r->provider()->listDirectoryResponse(*directory, *output,
                                                          next_token);
          PageData result{lst, next_token};
          completed(result);
          r->done(result);
        },
        output);
  });
}

}  // namespace cloudstorage
