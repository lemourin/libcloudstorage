/*****************************************************************************
 * AuthorizeRequest.cpp : AuthorizeRequest implementation
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

#include "AuthorizeRequest.h"

#include "CloudProvider/CloudProvider.h"

using namespace std::placeholders;

namespace cloudstorage {

AuthorizeRequest::AuthorizeRequest(std::shared_ptr<CloudProvider> p,
                                   AuthorizationFlow callback)
    : Request(p,
              [=](EitherError<void> e) {
                provider()->auth_callback()->done(*provider(), e);
              },
              std::bind(&AuthorizeRequest::resolve, this, _1, callback)),
      state_(provider()->auth()->state()),
      server_cancelled_() {
  if (!provider()->auth_callback()) {
    util::log("CloudProvider's callback can't be null.");
    std::terminate();
  }
}

AuthorizeRequest::~AuthorizeRequest() { cancel(); }

void AuthorizeRequest::resolve(Request::Pointer request,
                               AuthorizationFlow callback) {
  auto on_complete = [=](EitherError<void> result) {
    std::unique_lock<std::mutex> lock(provider()->current_authorization_mutex_);
    while (!provider()->auth_callbacks_.empty()) {
      {
        auto v = std::move(provider()->auth_callbacks_.begin()->second);
        provider()->auth_callbacks_.erase(provider()->auth_callbacks_.begin());
        lock.unlock();
        for (auto&& r : v) r(result);
      }
      lock.lock();
    }
    provider()->current_authorization_ = nullptr;
    lock.unlock();
    request->done(result);
  };
  callback ? callback(std::static_pointer_cast<AuthorizeRequest>(request),
                      on_complete)
           : oauth2Authorization(on_complete);
}

void AuthorizeRequest::sendCancel() {
  std::unique_lock<std::mutex> lock(lock_);
  auto auth_server = std::move(auth_server_);
  server_cancelled_ = true;
  lock.unlock();
  if (auth_server) {
    class Request : public IHttpServer::IRequest {
     public:
      Request(const std::string& state) : state_(state) {}
      const char* get(const std::string& name) const override {
        if (name == "error") return "cancelled";
        if (name == "accepted") return "false";
        if (name == "state") return state_.c_str();
        return nullptr;
      }
      const char* header(const std::string&) const override { return nullptr; }
      std::string url() const override { return "/"; }
      std::string method() const override { return "GET"; }
      IHttpServer::IResponse::Pointer response(
          int, const IHttpServer::IResponse::Headers&, int,
          IHttpServer::IResponse::ICallback::Pointer) const override {
        return nullptr;
      }

     private:
      std::string state_;
    };
    auth_server->callback()->handle(Request(state_));
  }
}

void AuthorizeRequest::cancel() {
  {
    std::lock_guard<std::mutex> lock(provider_mutex_);
    if (provider()) {
      std::lock_guard<std::mutex> lock(
          provider()->current_authorization_mutex_);
      if (!provider()->auth_callbacks_.empty()) return;
    }
  }
  sendCancel();
  Request::cancel();
}

void AuthorizeRequest::set_server(std::shared_ptr<IHttpServer> p) {
  std::unique_lock<std::mutex> lock(lock_);
  auth_server_ = p;
  if (p && server_cancelled_) {
    lock.unlock();
    sendCancel();
  }
}

void AuthorizeRequest::oauth2Authorization(AuthorizeCompleted complete) {
  auto auth = provider()->auth();
  auto auth_callback = provider()->auth_callback();
  send([=](util::Output input) { return auth->refreshTokenRequest(*input); },
       [=](EitherError<Response> e) {
         if (auto r = e.right()) {
           try {
             auto lock = provider()->auth_lock();
             auth->set_access_token(auth->refreshTokenResponse(r->output()));
             lock.unlock();
             return complete(nullptr);
           } catch (const std::exception&) {
             return complete(Error{IHttpRequest::Failure, r->output().str()});
           }
         } else if ((!IHttpRequest::isClientError(e.left()->code_) &&
                     e.left()->code_ != IHttpRequest::Aborted) ||
                    auth_callback->userConsentRequired(*provider()) ==
                        ICloudProvider::IAuthCallback::Status::None) {
           return complete(e.left());
         }
         auto code = [=](EitherError<std::string> authorization_code) {
           if (authorization_code.left())
             return complete(authorization_code.left());
           auth->set_authorization_code(*authorization_code.right());
           send(
               [=](util::Output input) {
                 return auth->exchangeAuthorizationCodeRequest(*input);
               },
               [=](EitherError<Response> e) {
                 if (e.left()) return complete(e.left());
                 try {
                   auto lock = provider()->auth_lock();
                   auth->set_access_token(
                       auth->exchangeAuthorizationCodeResponse(
                           e.right()->output()));
                   lock.unlock();
                   complete(nullptr);
                 } catch (const std::exception&) {
                   complete(
                       Error{IHttpRequest::Failure, e.right()->output().str()});
                 }
               });
         };
         set_server(auth->requestAuthorizationCode(code));
       });
}

SimpleAuthorization::SimpleAuthorization(std::shared_ptr<CloudProvider> p)
    : AuthorizeRequest(p, [=](AuthorizeRequest::Pointer r,
                              AuthorizeRequest::AuthorizeCompleted complete) {
        if (p->auth_callback()->userConsentRequired(*p) !=
            ICloudProvider::IAuthCallback::Status::WaitForAuthorizationCode) {
          return complete(
              Error{IHttpRequest::Unauthorized, "invalid credentials"});
        }
        auto code = [=](EitherError<std::string> code) {
          (void)r;
          if (code.left())
            complete(code.left());
          else
            complete(p->unpackCredentials(*code.right())
                         ? EitherError<void>(nullptr)
                         : EitherError<void>(
                               Error{IHttpRequest::Failure, "invalid code"}));
        };
        set_server(r->provider()->auth()->requestAuthorizationCode(code));
      }) {}

}  // namespace cloudstorage
