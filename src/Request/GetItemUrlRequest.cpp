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
    : Request(p, callback, [=](Request<EitherError<std::string>>::Pointer r) {
        if (item->type() == IItem::FileType::Directory)
          return r->done(Error{IHttpRequest::ServiceUnavailable,
                               "url not provided for directory"});
        std::stringstream dummy;
        if (!provider()->getItemUrlRequest(*item, dummy)) {
          auto url = static_cast<Item*>(item.get())->url();
          if (url.empty())
            return r->done(
                Error{IHttpRequest::ServiceUnavailable, "url not provided"});
          else
            return r->done(url);
        }
        r->request(
            [=](util::Output input) {
              return provider()->getItemUrlRequest(*item, *input);
            },
            [=](EitherError<Response> e) {
              try {
                if (e.left())
                  r->done(e.left());
                else {
                  auto url = provider()->getItemUrlResponse(
                      *item, e.right()->headers(), e.right()->output());
                  static_cast<Item*>(item.get())->set_url(url);
                  r->done(url);
                }
              } catch (std::exception) {
                r->done(
                    Error{IHttpRequest::Failure, e.right()->output().str()});
              }
            });
      }) {}

GetItemUrlRequest::~GetItemUrlRequest() { cancel(); }

}  // namespace cloudstorage
