/*****************************************************************************
 * FourShared.cpp
 *
 *****************************************************************************
 * Copyright (C) 2018 VideoLAN
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
#include "FourShared.h"

#include <json/json.h>
#include <cstring>

#include "Request/DownloadFileRequest.h"
#include "Utility/Item.h"
#include "Utility/Utility.h"

namespace {
std::atomic_int oauth_nonce;
}  // namespace

namespace cloudstorage {

struct AuthSession {
  std::string oauth_session_;
  std::string oauth_authenticity_;
};

std::string find_authenticity_token(const std::string &page) {
  auto token = R"("oauth_authenticity_token" value=")";
  auto start = page.find(token) + strlen(token);
  auto end = page.find('"', start);
  return std::string(page.begin() + start, page.begin() + end);
}

void do_oauth_fetch_session(
    const AuthorizeRequest::Pointer &r, const std::string &username,
    const std::string &password, const std::string &oauth_token,
    const std::function<void(EitherError<AuthSession>)> &complete) {
  auto p = r->provider().get();
  r->send(
      [=](util::Output) {
        auto r =
            p->http()->create(p->endpoint() + "/oauth/authorize", "GET", false);
        r->setParameter("oauth_token", oauth_token);
        r->setHeaderParameter("cookie",
                              "Login=" + username + "; Password=" + password);
        return r;
      },
      [=](EitherError<Response> e) {
        if (e.left()) return complete(e.left());
        if (IHttpRequest::isRedirect(e.right()->http_code())) {
          return complete(AuthSession{"ok", ""});
        }
        auto cookies = e.right()->headers().equal_range("set-cookie");
        std::string oauth_session;
        for (auto it = cookies.first; it != cookies.second; it++) {
          auto dict = util::parse_cookie(it->second);
          auto entry = dict.find("oauth_session");
          if (entry != dict.end()) oauth_session = entry->second;
        }
        AuthSession session;
        session.oauth_session_ = oauth_session;
        session.oauth_authenticity_ =
            find_authenticity_token(e.right()->output().str());
        complete(session);
      });
}

void do_oauth_consent(const AuthorizeRequest::Pointer &r,
                      const std::string &username, const std::string &password,
                      const std::string &oauth_token,
                      const AuthSession &session,
                      const std::function<void(EitherError<void>)> &complete) {
  auto p = r->provider().get();
  r->send(
      [=](util::Output input) {
        auto r = p->http()->create(p->endpoint() + "/oauth/authorize", "POST",
                                   false);
        r->setHeaderParameter(
            "cookie", "Login=" + username + "; Password=" + password +
                          "; oauth_session=" + session.oauth_session_ + ";");
        *input << "oauth_authenticity_token=" << session.oauth_authenticity_
               << "&oauth_token=" << oauth_token
               << "&oauth_callback=" << util::Url::escape("http://localhost")
               << "&allow=true";
        return r;
      },
      [=](EitherError<Response> e) {
        if (e.left()) return complete(e.left());
        complete(nullptr);
      });
}

void do_oauth_authorize(
    const AuthorizeRequest::Pointer &r, const std::string &username,
    const std::string &password, const std::string &oauth_token,
    const std::function<void(EitherError<void>)> &complete) {
  do_oauth_fetch_session(r, username, password, oauth_token,
                         [=](EitherError<AuthSession> e) {
                           if (e.left()) return complete(e.left());
                           if (e.right()->oauth_session_ == "ok")
                             return complete(nullptr);
                           do_oauth_consent(r, username, password, oauth_token,
                                            *e.right(), complete);
                         });
}

void do_oauth_token(
    const AuthorizeRequest::Pointer &r, const std::string &oauth_token,
    const std::string &oauth_token_secret,
    const std::function<void(EitherError<std::pair<std::string, std::string>>)>
        &complete) {
  auto p = r->provider().get();
  r->send(
      [=](util::Output input) {
        auto r = p->http()->create("https://api.4shared.com/v1_2/oauth/token",
                                   "POST");
        *input << "oauth_consumer_key=" << p->auth()->client_id()
               << "&oauth_nonce=" << oauth_nonce++
               << "&oauth_signature_method=PLAINTEXT"
               << "&oauth_timestamp="
               << std::chrono::duration_cast<std::chrono::seconds>(
                      std::chrono::system_clock::now().time_since_epoch())
                      .count()
               << "&oauth_token=" << oauth_token
               << "&oauth_token_secret=" << oauth_token_secret
               << "&oauth_signature="
               << util::Url::escape(p->auth()->client_secret() + "&" +
                                    oauth_token_secret);
        return r;
      },
      [=](EitherError<Response> e) {
        if (e.left()) return complete(e.left());
        auto result = util::parse_form(e.right()->output().str());
        complete(std::make_pair(result["oauth_token"],
                                result["oauth_token_secret"]));
      });
}

void do_oauth_initiate(
    const AuthorizeRequest::Pointer &r,
    const std::function<void(EitherError<std::pair<std::string, std::string>>)>
        &complete) {
  auto p = r->provider().get();
  r->send(
      [=](util::Output stream) {
        auto request =
            p->http()->create(p->endpoint() + "/oauth/initiate", "POST");
        *stream << "oauth_consumer_key=" << p->auth()->client_id()
                << "&oauth_signature_method=PLAINTEXT"
                << "&oauth_signature=" << p->auth()->client_secret() << "%26"
                << "&oauth_callback=http://localhost"
                << "&oauth_nonce=" << oauth_nonce++ << "&oauth_timestamp="
                << std::chrono::duration_cast<std::chrono::seconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count();
        return request;
      },
      [=](EitherError<Response> e) {
        if (e.left()) return complete(e.left());
        auto data = util::parse_form(e.right()->output().str());
        complete(
            std::make_pair(data["oauth_token"], data["oauth_token_secret"]));
      });
}

template <class Result>
void do_get_cookie(
    const typename Request<Result>::Pointer &r, const std::string &username,
    const std::string &password,
    const std::function<void(EitherError<std::string>)> &complete) {
  auto p = r->provider().get();
  r->send(
      [=](util::Output input) {
        auto r = p->http()->create("https://www.4shared.com/web/login", "POST",
                                   false);
        *input << "login=" << util::Url::escape(username)
               << "&password=" << util::Url::escape(password)
               << "&remember=true";
        return r;
      },
      [=](EitherError<Response> e) {
        if (e.left()) return complete(e.left());
        auto cookies = e.right()->headers().equal_range("set-cookie");
        std::string login, password;
        for (auto it = cookies.first; it != cookies.second; it++) {
          auto dict = util::parse_cookie(it->second);
          auto entry_login = dict.find("Login");
          if (entry_login != dict.end()) login = entry_login->second;
          auto entry_password = dict.find("Password");
          if (entry_password != dict.end()) password = entry_password->second;
        }
        if (login.empty() || password.empty())
          complete(Error{IHttpRequest::Unauthorized,
                         util::Error::INVALID_CREDENTIALS});
        else
          complete("Login=" + login + "; " + "Password=" + password);
      });
}

void do_oauth(
    const AuthorizeRequest::Pointer &r, const std::string &username,
    const std::string &password,
    const std::function<void(EitherError<std::pair<std::string, std::string>>)>
        &complete) {
  do_oauth_initiate(r, [=](EitherError<std::pair<std::string, std::string>> d) {
    if (d.left()) return complete(d.left());
    do_oauth_authorize(
        r, username, password, d.right()->first, [=](EitherError<void> e) {
          if (e.left()) return complete(e.left());
          do_oauth_token(r, d.right()->first, d.right()->second, complete);
        });
  });
}

void do_user_interaction(
    const AuthorizeRequest::Pointer &r,
    const std::function<void(EitherError<std::string>)> &complete) {
  if (r->provider()->auth_callback()->userConsentRequired(*r->provider()) !=
      ICloudProvider::IAuthCallback::Status::WaitForAuthorizationCode) {
    return complete(
        Error{IHttpRequest::Unauthorized, util::Error::INVALID_CREDENTIALS});
  }
  r->set_server(r->provider()->auth()->requestAuthorizationCode(complete));
}

void do_authorize(
    const AuthorizeRequest::Pointer &r, const std::string &username,
    const std::string &password,
    const std::function<void(EitherError<std::pair<std::string, std::string>>)>
        &complete) {
  do_oauth(r, username, password,
           [=](EitherError<std::pair<std::string, std::string>> e) {
             if (e.left()) {
               if (!IHttpRequest::isClientError(e.left()->code_))
                 return complete(e.left());
               do_user_interaction(r, [=](EitherError<std::string> e) {
                 if (e.left()) return complete(e.left());
                 r->make_subrequest(
                     &CloudProvider::exchangeCodeAsync, *e.right(),
                     [=](EitherError<Token> e) {
                       if (e.left()) return complete(e.left());
                       auto d = CloudProvider::credentialsFromString(
                           e.right()->token_);
                       r->provider()->unpackCredentials(e.right()->token_);
                       do_oauth(r, d["username"].asString(),
                                d["password"].asString(), complete);
                     });
               });
             } else {
               complete(e.right());
             }
           });
}

FourShared::FourShared() : CloudProvider(util::make_unique<Auth>()) {}

IItem::Pointer FourShared::rootDirectory() const {
  return util::make_unique<Item>("root", util::FileId(true, "root"),
                                 IItem::UnknownSize, IItem::UnknownTimeStamp,
                                 IItem::FileType::Directory);
}

void FourShared::initialize(ICloudProvider::InitData &&data) {
  if (data.token_.empty())
    data.token_ = credentialsToString(Json::Value(Json::objectValue));
  unpackCredentials(data.token_);
  setWithHint(data.hints_, "root_id", [this](std::string v) {
    auto lock = auth_lock();
    root_id_ = v;
  });
  CloudProvider::initialize(std::move(data));
}

std::string FourShared::name() const { return "4shared"; }

std::string FourShared::endpoint() const {
  return "https://api.4shared.com/v1_2";
}

void FourShared::authorizeRequest(IHttpRequest &request) const {
  auto lock = auth_lock();
  std::stringstream data;
  request.setHeaderParameter(
      "cookie", "Login=" + username_ + "; Password=" + password_ + ";");
  std::stringstream auth_header;
  auth_header << "OAuth oauth_token=\"" << oauth_token_
              << R"(", oauth_consumer_key=")" << auth()->client_id()
              << R"(", oauth_signature_method="PLAINTEXT", oauth_signature=")"
              << auth()->client_secret() << "&" << oauth_token_secret_
              << R"(", oauth_timestamp=")"
              << std::chrono::duration_cast<std::chrono::seconds>(
                     std::chrono::system_clock::now().time_since_epoch())
                     .count()
              << R"(", oauth_nonce=")" << oauth_nonce++ << '"';
  request.setHeaderParameter("Authorization", auth_header.str());
}

bool FourShared::reauthorize(
    int code, const IHttpRequest::HeaderParameters &headers) const {
  auto lock = auth_lock();
  bool empty =
      root_id_.empty() || oauth_token_.empty() || oauth_token_secret_.empty();
  lock.unlock();
  return empty || CloudProvider::reauthorize(code, headers);
}

ICloudProvider::Hints FourShared::hints() const {
  auto lock = auth_lock();
  Hints result = {{"root_id", root_id_},
                  {"oauth_token", oauth_token_},
                  {"oauth_token_secret", oauth_token_secret_}};
  lock.unlock();
  auto t = CloudProvider::hints();
  result.insert(t.begin(), t.end());
  return result;
}

std::string FourShared::token() const {
  auto lock = auth_lock();
  Json::Value json;
  json["username"] = username_;
  json["password"] = password_;
  return credentialsToString(json);
}

AuthorizeRequest::Pointer FourShared::authorizeAsync() {
  return std::make_shared<AuthorizeRequest>(
      shared_from_this(), [=](AuthorizeRequest::Pointer r,
                              AuthorizeRequest::AuthorizeCompleted complete) {
        do_authorize(r, username(), password(),
                     [=](EitherError<std::pair<std::string, std::string>> e) {
                       if (e.left()) return complete(e.left());
                       {
                         auto lock = auth_lock();
                         oauth_token_ = e.right()->first;
                         oauth_token_secret_ = e.right()->second;
                       }
                       auto take_root_id = [=](const EitherError<Response> &e) {
                         if (e.left()) return complete(e.left());
                         try {
                           auto json = util::json::from_string(
                               e.right()->output().str());
                           {
                             auto lock = auth_lock();
                             root_id_ = json["rootFolderId"].asString();
                           }
                           complete(nullptr);
                         } catch (const Json::Exception &e) {
                           complete(Error{IHttpRequest::Failure, e.what()});
                         }
                       };
                       r->request(
                           [=](util::Output) {
                             return http()->create(endpoint() + "/user");
                           },
                           [=](EitherError<Response> e) { take_root_id(e); });
                     });
      });
}

IHttpRequest::Pointer FourShared::listDirectoryRequest(
    const IItem &item, const std::string &page_token, std::ostream &) const {
  return http()->create(endpoint() + "/folders/" +
                        (item.id() == rootDirectory()->id()
                             ? root_id()
                             : util::FileId(item.id()).id_) +
                        (page_token.empty() ? "/children" : "/files"));
}

IItem::List FourShared::listDirectoryResponse(
    const IItem &, std::istream &data, std::string &next_page_token) const {
  auto json = util::json::from_stream(data);
  IItem::List result;
  if (json.isMember("folders")) {
    for (const auto &d : json["folders"]) {
      result.push_back(toItem(d));
    }
    next_page_token = "files";
  }
  if (json.isMember("files"))
    for (const auto &d : json["files"]) {
      result.push_back(toItem(d));
    }
  return result;
}

IHttpRequest::Pointer FourShared::uploadFileRequest(const IItem &directory,
                                                    const std::string &filename,
                                                    std::ostream &,
                                                    std::ostream &) const {
  auto directory_id = util::FileId(directory.id());
  auto parent_id =
      directory.id() == rootDirectory()->id() ? root_id() : directory_id.id_;
  auto r = http()->create("https://upload.4shared.com/v1_2/files", "POST");
  r->setParameter("folderId", parent_id);
  r->setParameter("fileName", util::Url::escape(filename));
  r->setHeaderParameter("Content-Type", "application/octet-stream");
  return r;
}

IHttpRequest::Pointer FourShared::deleteItemRequest(const IItem &item,
                                                    std::ostream &) const {
  auto id = util::FileId(item.id());
  return http()->create(
      endpoint() + "/" + (id.folder_ ? "folders" : "files") + "/" + id.id_,
      "DELETE");
}

IHttpRequest::Pointer FourShared::createDirectoryRequest(
    const IItem &parent, const std::string &name, std::ostream &stream) const {
  auto r = http()->create(endpoint() + "/folders", "POST");
  r->setHeaderParameter("Content-Type", "application/x-www-form-urlencoded");
  stream << "parentId="
         << (parent.id() == rootDirectory()->id()
                 ? root_id()
                 : util::FileId(parent.id()).id_)
         << "&name=" << util::Url::escape(name);
  return r;
}

IHttpRequest::Pointer FourShared::moveItemRequest(const IItem &source,
                                                  const IItem &destination,
                                                  std::ostream &stream) const {
  auto file_id = util::FileId(source.id());
  auto id = source.id() == rootDirectory()->id() ? root_id() : file_id.id_;
  auto dest_file_id = util::FileId(destination.id());
  auto dest_id =
      destination.id() == rootDirectory()->id() ? root_id() : dest_file_id.id_;
  auto r = http()->create(
      endpoint() + (file_id.folder_ ? "/folders/" : "/files/") + id + "/move",
      "POST");
  r->setHeaderParameter("Content-Type", "application/x-www-form-urlencoded");
  stream << "folderId=" << dest_id;
  return r;
}

IHttpRequest::Pointer FourShared::renameItemRequest(
    const IItem &item, const std::string &name, std::ostream &stream) const {
  auto file_id = util::FileId(item.id());
  auto r = http()->create(
      endpoint() + (file_id.folder_ ? "/folders/" : "/files/") + file_id.id_,
      "PUT");
  r->setHeaderParameter("Content-Type", "application/x-www-form-urlencoded");
  stream << "name=" << util::Url::escape(name);
  return r;
}

IItem::Pointer FourShared::toItem(const Json::Value &d) const {
  if (d.isMember("folderLink"))
    return util::make_unique<Item>(
        d["name"].asString(), util::FileId(true, d["id"].asString()),
        IItem::UnknownSize, util::parse_time(d["modified"].asString()),
        IItem::FileType::Directory);
  else
    return util::make_unique<Item>(
        d["name"].asString(), util::FileId(false, d["id"].asString()),
        d["size"].asUInt64(), util::parse_time(d["modified"].asString()),
        Item::fromMimeType(d["mimeType"].asString()));
}

std::string FourShared::root_id() const {
  auto lock = auth_lock();
  return root_id_;
}

IHttpRequest::Pointer FourShared::getGeneralDataRequest(std::ostream &) const {
  return http()->create(endpoint() + "/user");
}

GeneralData FourShared::getGeneralDataResponse(std::istream &response) const {
  auto d = util::json::from_stream(response);
  GeneralData result;
  result.space_total_ = d["totalSpace"].asUInt64();
  result.space_used_ = result.space_total_ - d["freeSpace"].asUInt64();
  result.username_ = d["email"].asString();
  return result;
}

ICloudProvider::DownloadFileRequest::Pointer FourShared::downloadFileAsync(
    IItem::Pointer i, IDownloadFileCallback::Pointer cb, Range range) {
  return std::make_shared<DownloadFileFromUrlRequest>(shared_from_this(), i, cb,
                                                      range)
      ->run();
}

IHttpRequest::Pointer FourShared::getItemDataRequest(const std::string &id,
                                                     std::ostream &) const {
  auto fid = util::FileId(id);
  auto disk_id = fid.id_ == rootDirectory()->id() ? root_id() : fid.id_;
  return http()->create(endpoint() + (fid.folder_ ? "/folders/" : "/files/") +
                        disk_id);
}

IItem::Pointer FourShared::getItemDataResponse(std::istream &response) const {
  auto json = util::json::from_stream(response);
  return toItem(json);
}

bool FourShared::unpackCredentials(const std::string &code) {
  auto lock = auth_lock();
  try {
    auto json = credentialsFromString(code);
    username_ = json["username"].asString();
    password_ = json["password"].asString();
    return true;
  } catch (const Json::Exception &) {
    return false;
  }
}

std::string FourShared::username() const {
  auto lock = auth_lock();
  return username_;
}

std::string FourShared::password() const {
  auto lock = auth_lock();
  return password_;
}

ICloudProvider::ExchangeCodeRequest::Pointer FourShared::exchangeCodeAsync(
    const std::string &token, ExchangeCodeCallback cb) {
  auto resolver = [=](Request<EitherError<Token>>::Pointer r) {
    auto json = credentialsFromString(token);
    do_get_cookie<EitherError<Token>>(
        r, json["username"].asString(), json["password"].asString(),
        [=](EitherError<std::string> e) {
          if (e.left()) return r->done(e.left());
          auto data = util::parse_cookie(*e.right());
          Json::Value json;
          json["username"] = data["Login"];
          json["password"] = data["Password"];
          r->done(Token{credentialsToString(json), ""});
        });
  };
  return std::make_shared<Request<EitherError<Token>>>(shared_from_this(), cb,
                                                       resolver)
      ->run();
}

ICloudProvider::GetItemUrlRequest::Pointer FourShared::getItemUrlAsync(
    IItem::Pointer item, GetItemUrlCallback callback) {
  using CurrentRequest = Request<EitherError<std::string>>;
  auto get_item_url = [=](CurrentRequest::Pointer r,
                          std::function<void(EitherError<std::string>)> cb) {
    r->request(
        [=](util::Output) {
          return http()->create(endpoint() + "/files/" +
                                util::FileId(item->id()).id_);
        },
        [=](EitherError<Response> e) {
          if (e.left()) {
            cb(e.left());
          } else {
            try {
              auto json = util::json::from_stream(e.right()->output());
              cb(json["downloadPage"].asString());
            } catch (const Json::Exception &e) {
              cb(Error{IHttpRequest::Failure, e.what()});
            }
          }
        });
  };
  auto resolve_redirect =
      [=](CurrentRequest::Pointer r, const std::string &url,
          std::function<void(EitherError<std::string>)> cb) {
        r->send(
            [=](util::Output) { return http()->create(url, "HEAD", false); },
            [=](EitherError<Response> e) {
              if (e.left()) return cb(e.left());
              auto d = e.right()->headers().find("location");
              if (d == e.right()->headers().end())
                cb(Error{IHttpRequest::Failure,
                         util::Error::UNKNOWN_RESPONSE_RECEIVED});
              else
                cb(d->second);
            });
      };
  auto find_cookie = [=](CurrentRequest::Pointer r, const std::string &url,
                         std::function<void(EitherError<std::string>)> cb) {
    r->send([=](util::Output) { return http()->create(url, "HEAD", false); },
            [=](EitherError<Response> e) {
              if (e.left()) return cb(e.left());
              auto cookies = e.right()->headers().equal_range("set-cookie");
              for (auto it = cookies.first; it != cookies.second; it++) {
                auto dict = util::parse_cookie(it->second);
                auto entry = dict.find("cd1v");
                if (entry != dict.end()) return cb(entry->second);
              }
              cb(Error{IHttpRequest::Failure,
                       util::Error::UNKNOWN_RESPONSE_RECEIVED});
            });
  };
  auto find_link = [=](const std::string &page) {
    auto search = R"(id="baseDownloadLink" value=")";
    auto begin = page.find(search) + strlen(search);
    auto end = page.find('"', begin);
    return std::string(page.begin() + begin, page.begin() + end);
  };
  auto get_url = [=](CurrentRequest::Pointer r, const std::string &url,
                     const std::string &cookie,
                     std::function<void(EitherError<std::string>)> cb) {
    auto d = util::Url(url).path();
    auto clipped = d.substr(d.find('/', 1));
    r->send(
        [=](util::Output) {
          auto r = http()->create("https://www.4shared.com/get/" + clipped,
                                  "GET", false);
          r->setHeaderParameter("cookie", "cd1v=" + cookie + ";");
          return r;
        },
        [=](EitherError<Response> e) {
          if (e.left()) return cb(e.left());
          cb(find_link(e.right()->output().str()));
        });
  };
  return std::make_shared<Request<EitherError<std::string>>>(
             shared_from_this(), callback,
             [=](Request<EitherError<std::string>>::Pointer r) {
               get_item_url(r, [=](EitherError<std::string> e) {
                 if (e.left()) return r->done(e.left());
                 resolve_redirect(
                     r, *e.right(), [=](EitherError<std::string> e) {
                       if (e.left()) return r->done(e.left());
                       auto redirect = *e.right();
                       find_cookie(r, *e.right(),
                                   [=](EitherError<std::string> e) {
                                     if (e.left()) return r->done(e.left());
                                     get_url(r, redirect, *e.right(),
                                             [=](EitherError<std::string> e) {
                                               r->done(e);
                                             });
                                   });
                     });
               });
             })
      ->run();
}

void FourShared::Auth::initialize(IHttp *http, IHttpServerFactory *factory) {
  cloudstorage::Auth::initialize(http, factory);
  if (client_id().empty()) {
    set_client_id("2d31bf7c54c8f860c445412b0f463a8e");
    set_client_secret("0230788b79b42fb73fb07c375fb020d57c505f37");
  }
}

}  // namespace cloudstorage
