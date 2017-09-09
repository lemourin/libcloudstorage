/*****************************************************************************
 * GetItemUrlRequest.cpp
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
#include "GetItemUrlRequest.h"

#include "CloudProvider/CloudProvider.h"
#include "Utility/Item.h"

namespace cloudstorage {

GetItemUrlRequest::GetItemUrlRequest(std::shared_ptr<CloudProvider> p,
                                     IItem::Pointer item,
                                     GetItemUrlCallback callback)
    : Request(p) {
  set(
      [=](Request<EitherError<std::string>>::Pointer r) {
        if (item->type() == IItem::FileType::Directory)
          return r->done(Error{IHttpRequest::ServiceUnavailable,
                               "url not provided for directory"});
        auto url = static_cast<Item*>(item.get())->url();
        if (!url.empty()) return r->done(url);
        auto output = std::make_shared<std::stringstream>();
        r->sendRequest(
            [=](util::Output input) {
              return provider()->getItemUrlRequest(*item, *input);
            },
            [=](EitherError<util::Output> e) {
              try {
                if (e.left())
                  r->done(e.left());
                else
                  r->done(provider()->getItemUrlResponse(*output));
              } catch (std::exception) {
                r->done(Error{IHttpRequest::Failure, output->str()});
              }
            },
            output);
      },
      callback);
}

GetItemUrlRequest::~GetItemUrlRequest() { cancel(); }

}  // namespace cloudstorage
