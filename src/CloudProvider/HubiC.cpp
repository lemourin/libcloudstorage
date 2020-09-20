/*****************************************************************************
 * HubiC.cpp
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
#include "HubiC.h"

#include "Request/RecursiveRequest.h"
#include "Utility/Item.h"

namespace cloudstorage {

HubiC::HubiC() : CloudProvider(util::make_unique<Auth>()) {}

std::string HubiC::name() const { return "hubic"; }

std::string HubiC::endpoint() const { return "https://api.hubic.com/1.0"; }

void HubiC::authorizeRequest(IHttpRequest &request) const {
  if (request.method() == "HEAD") return;
  CloudProvider::authorizeRequest(request);
  request.setHeaderParameter("X-Auth-Token", openstack_token());
}

IItem::Pointer HubiC::rootDirectory() const {
  return util::make_unique<Item>("/", "", IItem::UnknownSize,
                                 IItem::UnknownTimeStamp,
                                 IItem::FileType::Directory);
}

bool HubiC::reauthorize(int code,
                        const IHttpRequest::HeaderParameters &h) const {
  return CloudProvider::reauthorize(code, h) || openstack_endpoint().empty() ||
         openstack_token().empty();
}

IHttpRequest::Pointer HubiC::listDirectoryRequest(const IItem &item,
                                                  const std::string &page_token,
                                                  std::ostream &) const {
  auto r = http()->create(openstack_endpoint() + "/default/");
  r->setParameter("format", "json");
  r->setParameter("marker", util::Url::escape(page_token));
  r->setParameter("path", util::Url::escape(item.id()));
  return r;
}

IHttpRequest::Pointer HubiC::uploadFileRequest(const IItem &directory,
                                               const std::string &filename,
                                               std::ostream &,
                                               std::ostream &) const {
  return http()->create(
      openstack_endpoint() + "/default/" +
          util::Url::escape(directory.id() +
                            (directory.id().empty() ? "" : "/") + filename),
      "PUT");
}

IHttpRequest::Pointer HubiC::downloadFileRequest(const IItem &item,
                                                 std::ostream &) const {
  return http()->create(openstack_endpoint() + "/default/" +
                        util::Url::escape(item.id()));
}

IHttpRequest::Pointer HubiC::createDirectoryRequest(const IItem &item,
                                                    const std::string &name,
                                                    std::ostream &) const {
  auto r = http()->create(
      openstack_endpoint() + "/default/" +
          util::Url::escape(item.id() + (item.id().empty() ? "" : "/") + name),
      "PUT");
  r->setHeaderParameter("Content-Type", "application/directory");
  return r;
}

IItem::Pointer HubiC::getItemDataResponse(std::istream &response) const {
  return toItem(util::json::from_stream(response)[0]);
}

std::string HubiC::getItemUrlResponse(const IItem &item,
                                      const IHttpRequest::HeaderParameters &,
                                      std::istream &) const {
  const int duration = 60 * 60;
  auto expires = std::to_string(time(nullptr) + duration);
  auto hmac_body =
      "GET\n" + expires + "\n" +
      util::Url(openstack_endpoint() + "/default/" + item.id()).path();
  auto signature =
      crypto()->hex(crypto()->hmac_sha1(openstack_token(), hmac_body));
  return openstack_endpoint() + "/default/" + util::Url::escape(item.id()) +
         "?temp_url_sig=" + signature + "&temp_url_expires=" + expires;
}

IItem::Pointer HubiC::uploadFileResponse(const IItem &item,
                                         const std::string &filename,
                                         uint64_t size, std::istream &) const {
  return util::make_unique<Item>(
      filename, item.id() + (item.id().empty() ? "" : "/") + filename, size,
      std::chrono::system_clock::now(), IItem::FileType::Unknown);
}

AuthorizeRequest::Pointer HubiC::authorizeAsync() {
  auto auth = [=](AuthorizeRequest::Pointer r,
                  AuthorizeRequest::AuthorizeCompleted complete) {
    r->oauth2Authorization([=](EitherError<void> auth_status) {
      if (auth_status.left()) return complete(auth_status.left());
      r->send(
          [=](util::Output) {
            auto r = http()->create(endpoint() + "/account/credentials", "GET");
            authorizeRequest(*r);
            return r;
          },
          [=](EitherError<Response> e) {
            if (e.left()) return complete(e.left());
            try {
              auto response = util::json::from_stream(e.right()->output());
              auto lock = auth_lock();
              openstack_endpoint_ = response["endpoint"].asString();
              openstack_token_ = response["token"].asString();
              lock.unlock();
              r->send(
                  [=](util::Output) {
                    auto r = http()->create(openstack_endpoint() + "/default",
                                            "POST");
                    authorizeRequest(*r);
                    r->setHeaderParameter("X-Container-Meta-Temp-URL-Key",
                                          openstack_token());
                    return r;
                  },
                  [=](EitherError<Response> e) {
                    (void)r;
                    if (e.left()) return complete(e.left());
                    complete(nullptr);
                  });
            } catch (const Json::Exception &) {
              complete(Error{IHttpRequest::Failure, e.right()->output().str()});
            }
          });
    });
  };
  return std::make_shared<AuthorizeRequest>(shared_from_this(), auth);
}

ICloudProvider::MoveItemRequest::Pointer HubiC::moveItemAsync(
    IItem::Pointer source, IItem::Pointer destination,
    MoveItemCallback callback) {
  using Request = RecursiveRequest<EitherError<IItem>>;
  auto visitor = [=](Request::Pointer r, IItem::Pointer item,
                     Request::CompleteCallback callback) {
    auto l = getPath("/" + source->id()).length();
    std::string new_id = destination->id() +
                         (destination->id().empty() ? "" : "/") +
                         item->id().substr(l == 0 ? 0 : l);
    r->request(
        [=](util::Output) {
          auto r = http()->create(openstack_endpoint() + "/default/" +
                                      util::Url::escape(item->id()),
                                  "COPY");
          r->setHeaderParameter("Destination",
                                "/default/" + util::Url::escape(new_id));
          return r;
        },
        [=](EitherError<Response> e) {
          if (e.left()) return callback(e.left());
          r->request(
              [=](util::Output) {
                return http()->create(openstack_endpoint() + "/default/" +
                                          util::Url::escape(item->id()),
                                      "DELETE");
              },
              [=](EitherError<Response> e) {
                if (e.left()) return callback(e.left());
                IItem::Pointer renamed = std::make_shared<Item>(
                    getFilename(new_id), new_id, item->size(),
                    item->timestamp(), item->type());
                callback(renamed);
              });
        });
  };
  return std::make_shared<Request>(shared_from_this(), source, callback,
                                   visitor)
      ->run();
}

ICloudProvider::MoveItemRequest::Pointer HubiC::renameItemAsync(
    IItem::Pointer root, const std::string &name, RenameItemCallback callback) {
  using Request = RecursiveRequest<EitherError<IItem>>;
  auto new_prefix = (getPath("/" + root->id()) + "/" + name).substr(1);
  auto visitor = [=](Request::Pointer r, IItem::Pointer item,
                     Request::CompleteCallback callback) {
    std::string new_id = new_prefix + item->id().substr(root->id().length());
    r->request(
        [=](util::Output) {
          auto r = http()->create(openstack_endpoint() + "/default/" +
                                      util::Url::escape(item->id()),
                                  "COPY");
          r->setHeaderParameter("Destination",
                                "/default/" + util::Url::escape(new_id));
          return r;
        },
        [=](EitherError<Response> e) {
          if (e.left()) return callback(e.left());
          r->request(
              [=](util::Output) {
                return http()->create(openstack_endpoint() + "/default/" +
                                          util::Url::escape(item->id()),
                                      "DELETE");
              },
              [=](EitherError<Response> e) {
                if (e.left()) return callback(e.left());
                IItem::Pointer renamed = std::make_shared<Item>(
                    getFilename(new_id), new_id, item->size(),
                    item->timestamp(), item->type());
                callback(renamed);
              });
        });
  };
  return std::make_shared<Request>(shared_from_this(), root, callback, visitor)
      ->run();
}

ICloudProvider::DeleteItemRequest::Pointer HubiC::deleteItemAsync(
    IItem::Pointer item, DeleteItemCallback callback) {
  using Request = RecursiveRequest<EitherError<void>>;
  auto visitor = [=](Request::Pointer r, IItem::Pointer item,
                     Request::CompleteCallback callback) {
    r->request(
        [=](util::Output) {
          return http()->create(openstack_endpoint() + "/default/" +
                                    util::Url::escape(item->id()),
                                "DELETE");
        },
        [=](EitherError<Response> e) {
          if (e.left())
            callback(e.left());
          else
            callback(nullptr);
        });
  };
  return std::make_shared<Request>(shared_from_this(), item, callback, visitor)
      ->run();
}

ICloudProvider::GeneralDataRequest::Pointer HubiC::getGeneralDataAsync(
    GeneralDataCallback callback) {
  auto resolver = [=](Request<EitherError<GeneralData>>::Pointer r) {
    r->request(
        [=](util::Output) { return http()->create(endpoint() + "/account"); },
        [=](EitherError<Response> e) {
          if (e.left()) return r->done(e.left());
          r->request(
              [=](util::Output) {
                return http()->create(endpoint() + "/account/usage");
              },
              [=](EitherError<Response> d) {
                if (d.left()) return r->done(d.left());
                try {
                  auto json1 = util::json::from_stream(e.right()->output());
                  auto json2 = util::json::from_stream(d.right()->output());
                  GeneralData result;
                  result.username_ = json1["email"].asString();
                  result.space_total_ = json2["quota"].asUInt64();
                  result.space_used_ = json2["used"].asUInt64();
                  r->done(result);
                } catch (const Json::Exception &e) {
                  r->done(Error{IHttpRequest::Failure, e.what()});
                }
              });
        });
  };
  return std::make_shared<Request<EitherError<GeneralData>>>(shared_from_this(),
                                                             callback, resolver)
      ->run();
}

IHttpRequest::Pointer HubiC::getItemDataRequest(const std::string &id,
                                                std::ostream &) const {
  auto r = http()->create(openstack_endpoint() + "/default");
  r->setParameter("format", "json");
  r->setParameter("prefix", util::Url::escape(id));
  r->setParameter("delimiter", "/");
  r->setParameter("limit", "1");
  return r;
}

IHttpRequest::Pointer HubiC::getItemUrlRequest(const IItem &item,
                                               std::ostream &) const {
  if (!crypto()) return nullptr;
  const int duration = 60 * 60;
  auto r = http()->create(
      openstack_endpoint() + "/default/" + util::Url::escape(item.id()),
      "HEAD");
  auto expires = std::to_string(time(nullptr) + duration);
  auto hmac_body =
      "GET\n" + expires + "\n" +
      util::Url(openstack_endpoint() + "/default/" + item.id()).path();
  auto signature =
      crypto()->hex(crypto()->hmac_sha1(openstack_token(), hmac_body));
  r->setParameter("temp_url_sig", signature);
  r->setParameter("temp_url_expires", expires);
  return r;
}

IItem::List HubiC::listDirectoryResponse(const IItem &, std::istream &stream,
                                         std::string &next_page_token) const {
  auto json = util::json::from_stream(stream);
  IItem::List result;
  for (auto &&v : json)
    if (!v.isMember("subdir")) {
      result.push_back(toItem(v));
      next_page_token = v["name"].asString();
    }
  return result;
}

IItem::Pointer HubiC::createDirectoryResponse(const IItem &parent,
                                              const std::string &name,
                                              std::istream &) const {
  return std::make_shared<Item>(
      name, parent.id() + (parent.id().empty() ? "" : "/") + name,
      IItem::UnknownSize, IItem::UnknownTimeStamp, IItem::FileType::Directory);
}

IItem::Pointer HubiC::toItem(const Json::Value &v) const {
  auto item = std::make_shared<Item>(
      CloudProvider::getFilename(v["name"].asString()), v["name"].asString(),
      v["content_type"].asString() == "application/directory"
          ? IItem::UnknownSize
          : v["bytes"].asUInt(),
      util::parse_time(v["last_modified"].asString() + "Z"),
      v["content_type"].asString() == "application/directory"
          ? IItem::FileType::Directory
          : IItem::FileType::Unknown);
  return item;
}

std::string HubiC::openstack_endpoint() const {
  auto lock = auth_lock();
  return openstack_endpoint_;
}

std::string HubiC::openstack_token() const {
  auto lock = auth_lock();
  return openstack_token_;
}

void HubiC::Auth::initialize(IHttp *http, IHttpServerFactory *factory) {
  cloudstorage::Auth::initialize(http, factory);
  if (client_id().empty()) {
    set_client_id("api_hubic_kHQ5NUmE2yAAeiWfXPwQy2T53M6RP2fe");
    set_client_secret(
        "Xk1ezMMSGNeU4r0wv3jutleYX9XvXmgg8XVElJjqoDvlDw0KsC9U2tkSxJxYof8V");
  }
}

std::string HubiC::Auth::authorizeLibraryUrl() const {
  auto redirect = redirect_uri();
  if (!redirect.empty() && redirect.back() != '/') redirect += '/';
  return "https://api.hubic.com/oauth/auth?client_id=" + client_id() +
         "&response_type=code&redirect_uri=" + redirect + "&state=" + state() +
         "&scope=credentials.r%2Caccount.r%2Cusage.r";
}

IHttpRequest::Pointer HubiC::Auth::exchangeAuthorizationCodeRequest(
    std::ostream &arguments) const {
  auto redirect = redirect_uri();
  if (!redirect.empty() && redirect.back() != '/') redirect += '/';
  auto r = http()->create("https://api.hubic.com/oauth/token", "POST");
  arguments << "grant_type=authorization_code&redirect_uri="
            << util::Url::escape(redirect) << "&code=" << authorization_code()
            << "&client_id=" << client_id()
            << "&client_secret=" << client_secret();
  return r;
}

IHttpRequest::Pointer HubiC::Auth::refreshTokenRequest(
    std::ostream &arguments) const {
  auto r = http()->create("https://api.hubic.com/oauth/token", "POST");
  arguments << "grant_type=refresh_token&refresh_token="
            << access_token()->refresh_token_ << "&client_id=" << client_id()
            << "&client_secret=" << client_secret();
  return r;
}

IAuth::Token::Pointer HubiC::Auth::exchangeAuthorizationCodeResponse(
    std::istream &stream) const {
  auto json = util::json::from_stream(stream);
  return util::make_unique<Token>(Token{json["access_token"].asString(),
                                        json["refresh_token"].asString(), -1});
}

IAuth::Token::Pointer HubiC::Auth::refreshTokenResponse(
    std::istream &data) const {
  auto response = util::json::from_stream(data);
  Token::Pointer token = util::make_unique<Token>();
  token->token_ = response["access_token"].asString();
  token->refresh_token_ = access_token()->refresh_token_;
  token->expires_in_ = response["expires_in"].asInt();
  return token;
}

bool HubiC::Auth::requiresCodeExchange() const { return true; }

}  // namespace cloudstorage
