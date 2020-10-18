/*****************************************************************************
 * MoveItemRequest.cpp : MoveItemRequest implementation
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

#include "MoveItemRequest.h"

#include "CloudProvider/CloudProvider.h"

namespace cloudstorage {

MoveItemRequest::MoveItemRequest(std::shared_ptr<CloudProvider> p,
                                 const IItem::Pointer& source,
                                 const IItem::Pointer& destination,
                                 const MoveItemCallback& callback)
    : Request(std::move(p), callback, [=, this](Request::Pointer request) {
        if (destination->type() != IItem::FileType::Directory)
          return request->done(
              Error{IHttpRequest::Forbidden, util::Error::NOT_A_DIRECTORY});
        this->request(
            [=, this](util::Output stream) {
              return provider()->moveItemRequest(*source, *destination,
                                                 *stream);
            },
            [=, this](EitherError<Response> e) {
              if (e.left()) return request->done(e.left());
              try {
                request->done(provider()->moveItemResponse(
                    *source, *destination, e.right()->output()));
              } catch (const std::exception& e) {
                request->done(Error{IHttpRequest::Failure, e.what()});
              }
            });
      }) {}

MoveItemRequest::~MoveItemRequest() { cancel(); }

}  // namespace cloudstorage
