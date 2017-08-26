/*****************************************************************************
 * GetItemDataRequest.cpp : GetItemDataRequest implementation
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

#include "GetItemDataRequest.h"

#include "CloudProvider/CloudProvider.h"
#include "Utility/Utility.h"

namespace cloudstorage {

GetItemDataRequest::GetItemDataRequest(std::shared_ptr<CloudProvider> p,
                                       const std::string& id, Callback callback)
    : Request(p) {
  set([=](Request::Pointer request) {
    auto response_stream = std::make_shared<std::stringstream>();
    sendRequest(
        [=](util::Output input) {
          return provider()->getItemDataRequest(id, *input);
        },
        [=](EitherError<util::Output> r) {
          if (r.left()) {
            callback(r.left());
            request->done(r.left());

          } else {
            try {
              auto i = provider()->getItemDataResponse(*response_stream);
              callback(i);
              request->done(i);
            } catch (std::exception) {
              Error e{IHttpRequest::Failure, response_stream->str()};
              callback(e);
              request->done(e);
            }
          }
        },
        response_stream);
  });
}

GetItemDataRequest::~GetItemDataRequest() { cancel(); }

}  // namespace cloudstorage
