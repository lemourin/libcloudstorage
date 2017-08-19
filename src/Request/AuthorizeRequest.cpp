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

namespace cloudstorage {

AuthorizeRequest::AuthorizeRequest(std::shared_ptr<CloudProvider> p,
                                   AuthorizationFlow callback)
    : Request(p) {
  if (!provider()->auth_callback())
    throw std::logic_error("CloudProvider's callback can't be null.");
  set([=](Request::Ptr request) {
    auto on_complete = [=](EitherError<void> result) {
      std::unique_lock<std::mutex> lock(
          provider()->current_authorization_mutex_);
      while (!provider()->auth_callbacks_.empty()) {
        {
          auto v = std::move(provider()->auth_callbacks_.begin()->second);
          provider()->auth_callbacks_.erase(
              provider()->auth_callbacks_.begin());
          lock.unlock();
          for (auto&& r : v) r(result);
        }
        lock.lock();
      }
      provider()->current_authorization_ = nullptr;
      lock.unlock();
      provider()->auth_callback()->done(*provider(), result);
      request->done(result);
    };
    callback ? callback(std::static_pointer_cast<AuthorizeRequest>(request),
                        on_complete)
             : oauth2Authorization(on_complete);
  });
}

AuthorizeRequest::~AuthorizeRequest() { cancel(); }

void AuthorizeRequest::sendCancel() {
  std::unique_lock<std::mutex> lock(lock_);
  auto auth_server = std::move(auth_server_);
  lock.unlock();
  if (auth_server) {
    class Connection : public IHttpServer::IConnection {
     public:
      Connection(const std::string& state) : state_(state) {}
      ~Connection() {
        if (f_) f_();
      }

      const char* getParameter(const std::string& name) const {
        if (name == "error") return "cancelled";
        if (name == "accepted") return "false";
        if (name == "state") return state_.c_str();
        return nullptr;
      }
      const char* header(const std::string&) const { return nullptr; }
      std::string url() const { return "/"; }
      void onCompleted(CompletedCallback f) { f_ = f; }
      void suspend() {}
      void resume() {}

     private:
      std::string state_;
      CompletedCallback f_;
    };
    auto connection = std::make_shared<Connection>(provider()->auth()->state());
    auth_server->callback()->receivedConnection(*auth_server, connection.get());
  }
}

void AuthorizeRequest::cancel() {
  if (is_cancelled()) return;
  {
    std::lock_guard<std::mutex> lock(lock_);
    if (!provider()->auth_callbacks_.empty()) return;
  }
  sendCancel();
  Request::cancel();
}

void AuthorizeRequest::set_server(std::shared_ptr<IHttpServer> p) {
  {
    std::lock_guard<std::mutex> lock(lock_);
    auth_server_ = p;
  }
  if (p && is_cancelled()) sendCancel();
}

void AuthorizeRequest::oauth2Authorization(AuthorizeCompleted complete) {
  auto p = provider();
  auto input = std::make_shared<std::stringstream>(),
       output = std::make_shared<std::stringstream>(),
       error_stream = std::make_shared<std::stringstream>();
  auto r = p->auth()->refreshTokenRequest(*input);
  send(r.get(),
       [=](int code, util::Output, util::Output) {
         if (IHttpRequest::isSuccess(code)) {
           try {
             auto lock = p->auth_lock();
             p->auth()->set_access_token(
                 p->auth()->refreshTokenResponse(*output));
             lock.unlock();
             return complete(nullptr);
           } catch (std::exception e) {
             Error err{IHttpRequest::Failure, e.what()};
             return complete(err);
           }
         } else if (!IHttpRequest::isClientError(code) && r) {
           return complete(Error{code, error_stream->str()});
         }
         if (p->auth_callback()->userConsentRequired(*provider()) ==
             ICloudProvider::IAuthCallback::Status::WaitForAuthorizationCode) {
           auto code = [=](EitherError<std::string> authorization_code) {
             if (authorization_code.left())
               return complete(authorization_code.left());
             p->auth()->set_authorization_code(*authorization_code.right());
             auto input = std::make_shared<std::stringstream>(),
                  output = std::make_shared<std::stringstream>(),
                  error_stream = std::make_shared<std::stringstream>();
             auto r = p->auth()->exchangeAuthorizationCodeRequest(*input);
             send(r.get(),
                  [=](int code, util::Output, util::Output) {
                    if (IHttpRequest::isSuccess(code)) {
                      try {
                        auto lock = p->auth_lock();
                        p->auth()->set_access_token(
                            p->auth()->exchangeAuthorizationCodeResponse(
                                *output));
                        lock.unlock();
                        complete(nullptr);
                      } catch (std::exception e) {
                        complete(Error{IHttpRequest::Failure, e.what()});
                      }
                    } else {
                      complete(Error{code, error_stream->str()});
                    }
                  },
                  input, output, error_stream);
           };
           set_server(p->auth()->requestAuthorizationCode(code));
         } else {
           complete(Error{code, error_stream->str()});
         }
       },
       input, output, error_stream);
}

SimpleAuthorization::SimpleAuthorization(std::shared_ptr<CloudProvider> p)
    : AuthorizeRequest(p, [=](AuthorizeRequest::Ptr r,
                              AuthorizeRequest::AuthorizeCompleted complete) {
        if (p->auth_callback()->userConsentRequired(*p) !=
            ICloudProvider::IAuthCallback::Status::WaitForAuthorizationCode) {
          return complete(Error{IHttpRequest::Failure, "not waiting for code"});
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
