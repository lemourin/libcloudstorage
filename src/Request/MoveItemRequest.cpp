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
                                 IItem::Pointer source,
                                 IItem::Pointer destination,
                                 MoveItemCallback callback)
    : Request(p) {
  set(
      [=](Request::Pointer request) {
        if (destination->type() != IItem::FileType::Directory)
          return request->done(
              Error{IHttpRequest::Forbidden, "destination not a directory"});
        sendRequest(
            [=](util::Output stream) {
              return p->moveItemRequest(*source, *destination, *stream);
            },
            [=](EitherError<Response> e) {
              if (e.left()) return request->done(e.left());
              try {
                request->done(p->moveItemResponse(*source, *destination,
                                                  e.right()->output()));
              } catch (std::exception) {
                request->done(
                    Error{IHttpRequest::Failure, e.right()->output().str()});
              }
            });
      },
      callback);
}

MoveItemRequest::~MoveItemRequest() { cancel(); }

}  // namespace cloudstorage
