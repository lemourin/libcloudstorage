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
    : Request(p), state_(provider()->auth()->state()), server_cancelled_() {
  if (!provider()->auth_callback())
    throw std::logic_error("CloudProvider's callback can't be null.");
  set([=](Request::Pointer request) {
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
  if (is_cancelled()) return;
  {
    std::lock_guard<std::mutex> lock1(provider_mutex_);
    std::lock_guard<std::mutex> lock2(lock_);
    if (provider() && !provider()->auth_callbacks_.empty()) return;
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
  auto input = std::make_shared<std::stringstream>(),
       output = std::make_shared<std::stringstream>(),
       error_stream = std::make_shared<std::stringstream>();
  auto r = provider()->auth()->refreshTokenRequest(*input);
  auto auth = provider()->auth();
  auto auth_callback = provider()->auth_callback();
  send(r.get(),
       [=](IHttpRequest::Response response) {
         if (IHttpRequest::isSuccess(response.http_code_)) {
           try {
             auto lock = provider()->auth_lock();
             auth->set_access_token(auth->refreshTokenResponse(*output));
             lock.unlock();
             return complete(nullptr);
           } catch (std::exception) {
             Error err{IHttpRequest::Failure, output->str()};
             return complete(err);
           }
         } else if (!IHttpRequest::isClientError(response.http_code_) && r) {
           return complete(Error{response.http_code_, error_stream->str()});
         }
         if (auth_callback->userConsentRequired(*provider()) ==
             ICloudProvider::IAuthCallback::Status::WaitForAuthorizationCode) {
           auto code = [=](EitherError<std::string> authorization_code) {
             if (authorization_code.left())
               return complete(authorization_code.left());
             auth->set_authorization_code(*authorization_code.right());
             auto input = std::make_shared<std::stringstream>(),
                  output = std::make_shared<std::stringstream>(),
                  error_stream = std::make_shared<std::stringstream>();
             auto r = auth->exchangeAuthorizationCodeRequest(*input);
             send(r.get(),
                  [=](IHttpRequest::Response response) {
                    if (IHttpRequest::isSuccess(response.http_code_)) {
                      try {
                        auto lock = provider()->auth_lock();
                        auth->set_access_token(
                            auth->exchangeAuthorizationCodeResponse(*output));
                        lock.unlock();
                        complete(nullptr);
                      } catch (std::exception) {
                        complete(Error{IHttpRequest::Failure, output->str()});
                      }
                    } else {
                      complete(Error{response.http_code_, error_stream->str()});
                    }
                  },
                  input, output, error_stream);
           };
           set_server(auth->requestAuthorizationCode(code));
         } else {
           complete(Error{response.http_code_, error_stream->str()});
         }
       },
       input, output, error_stream);
}

SimpleAuthorization::SimpleAuthorization(std::shared_ptr<CloudProvider> p)
    : AuthorizeRequest(p, [=](AuthorizeRequest::Pointer r,
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
