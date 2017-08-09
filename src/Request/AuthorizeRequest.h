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

#include "IAuth.h"

namespace cloudstorage {

class AuthorizeRequest : public Request<EitherError<void>> {
 public:
  using Ptr = std::shared_ptr<AuthorizeRequest>;
  using AuthorizeCompleted = std::function<void(EitherError<void>)>;
  using AuthorizationFlow = std::function<void(
      std::shared_ptr<AuthorizeRequest>, AuthorizeCompleted)>;

  AuthorizeRequest(std::shared_ptr<CloudProvider>, AuthorizationFlow = nullptr);
  ~AuthorizeRequest();

  void oauth2Authorization(AuthorizeCompleted);
  void sendCancel();
  void cancel();
  void set_server(std::shared_ptr<IHttpServer>, AuthorizeCompleted);

 private:
  std::mutex lock_;
  std::shared_ptr<IHttpServer> auth_server_;
};

class SimpleAuthorization : public AuthorizeRequest {
 public:
  SimpleAuthorization(std::shared_ptr<CloudProvider>);
};

}  // namespace cloudstorage

#endif  // AUTHORIZEREQUEST_H
