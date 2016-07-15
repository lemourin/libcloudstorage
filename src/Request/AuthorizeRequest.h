/*****************************************************************************
 * AuthorizeRequest.h : AuthorizeRequest headers
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

#ifndef AUTHORIZEREQUEST_H
#define AUTHORIZEREQUEST_H

#include "Request.h"

namespace cloudstorage {

class AuthorizeRequest : public Request<bool> {
 public:
  using Pointer = std::shared_ptr<AuthorizeRequest>;
  using AuthorizationFlow = std::function<bool(AuthorizeRequest*)>;

  AuthorizeRequest(std::shared_ptr<CloudProvider>, AuthorizationFlow = nullptr);
  ~AuthorizeRequest();

  void cancel();
  bool oauth2Authorization();

 private:
  std::mutex mutex_;
  bool awaiting_authorization_code_;
  AuthorizationFlow callback_;
};

}  // namespace cloudstorage

#endif  // AUTHORIZEREQUEST_H
