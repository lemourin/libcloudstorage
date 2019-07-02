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

CreateDirectoryRequest::CreateDirectoryRequest(
    std::shared_ptr<CloudProvider> p, const IItem::Pointer& parent,
    const std::string& name, const CreateDirectoryCallback& callback)
    : Request(std::move(p), callback, [=](Request::Pointer request) {
        if (parent->type() != IItem::FileType::Directory) {
          Error e{IHttpRequest::Forbidden, util::Error::NOT_A_DIRECTORY};
          callback(e);
          return done(e);
        }
        this->request(
            [=](util::Output stream) {
              return provider()->createDirectoryRequest(*parent, name, *stream);
            },
            [=](EitherError<Response> e) {
              if (e.left()) return request->done(e.left());
              try {
                request->done(provider()->createDirectoryResponse(
                    *parent, name, e.right()->output()));
              } catch (const std::exception& e) {
                request->done(Error{IHttpRequest::Failure, e.what()});
              }
            });
      }) {}

CreateDirectoryRequest::~CreateDirectoryRequest() { cancel(); }

}  // namespace cloudstorage
