/*****************************************************************************
 * CloudFactory.cpp
 *
 *****************************************************************************
 * Copyright (C) 2016-2019 VideoLAN
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
#include "CloudFactory.h"
#include "HttpServer.h"
#include "Utility/Utility.h"

#include "LoginPage.h"

#include <algorithm>
#include <csignal>
#include <cstring>

namespace cloudstorage {

namespace {
struct HttpWrapper : public IHttp {
  HttpWrapper(std::shared_ptr<IHttp> http) : http_(http) {}

  IHttpRequest::Pointer create(const std::string& url,
                               const std::string& method,
                               bool follow_redirect) const override {
    return http_->create(url, method, follow_redirect);
  }

  std::shared_ptr<IHttp> http_;
};

struct CryptoWrapper : public ICrypto {
  CryptoWrapper(std::shared_ptr<ICrypto> crypto) : crypto_(crypto) {}

  std::string sha256(const std::string& message) override {
    return crypto_->sha256(message);
  }
  std::string hmac_sha256(const std::string& key,
                          const std::string& message) override {
    return crypto_->hmac_sha256(key, message);
  }
  std::string hmac_sha1(const std::string& key,
                        const std::string& message) override {
    return crypto_->hmac_sha1(key, message);
  }

  std::string hex(const std::string& hash) override {
    return crypto_->hex(hash);
  }

  std::shared_ptr<ICrypto> crypto_;
};

struct ThreadPoolWrapper : public IThreadPool {
  ThreadPoolWrapper(std::shared_ptr<IThreadPool> pool) : thread_pool_(pool) {}

  void schedule(const Task& f) override { thread_pool_->schedule(f); }

  std::shared_ptr<IThreadPool> thread_pool_;
};

struct AuthCallback : public ICloudProvider::IAuthCallback {
  AuthCallback(CloudFactory* factory) : factory_(factory) {}

  Status userConsentRequired(const ICloudProvider&) override {
    return Status::None;
  }

  void done(const ICloudProvider& p, EitherError<void> e) override {
    if (e.left()) {
      factory_->onCloudRemoved(p);
    }
  }

  CloudFactory* factory_;
};

struct HttpServerFactoryWrapper : public IHttpServerFactory {
  HttpServerFactoryWrapper(std::shared_ptr<IHttpServerFactory> factory)
      : factory_(factory) {}

  IHttpServer::Pointer create(IHttpServer::ICallback::Pointer cb,
                              const std::string& session_id,
                              IHttpServer::Type type) override {
    return factory_->create(cb, session_id, type);
  }

 private:
  std::shared_ptr<IHttpServerFactory> factory_;
};

#define MAKE_STRING(name) \
  std::string(reinterpret_cast<const char*>(name), name##_len)

struct HttpCallback : public IHttpServer::ICallback {
  HttpCallback(CloudFactory* factory) : factory_(factory) {}

  IHttpServer::IResponse::Pointer handle(
      const IHttpServer::IRequest& request) override {
    auto state = first_url_part(request.url());
    if (state == "favicon.ico") {
      return util::response_from_string(request, IHttpRequest::Ok, {},
                                        MAKE_STRING(cloud_png));
    } else if (state == "static" &&
               request.url().length() >= strlen("/static/")) {
      auto filename = request.url().substr(strlen("/static/"));
      std::string result;
      if (filename == "bootstrap.min.css")
        result = MAKE_STRING(bootstrap_min_css);
      else if (filename == "bootstrap.min.js")
        result = MAKE_STRING(bootstrap_min_js);
      else if (filename == "url.min.js")
        result = MAKE_STRING(url_min_js);
      else if (filename == "style.min.css")
        result = MAKE_STRING(style_min_css);
      else if (filename == "jquery.min.js")
        result = MAKE_STRING(jquery_min_js);
      else if (filename == "vlc-blue.png")
        result = MAKE_STRING(vlc_blue_png);
      else
        return util::response_from_string(request, IHttpRequest::NotFound, {},
                                          "Not found");
      return util::response_from_string(request, IHttpRequest::Ok, {}, result);
    }
    const char* code = request.get("code");
    if (code) {
      factory_->invoke([factory = factory_, state, code = std::string(code)] {
        factory->onCloudAuthenticationCodeReceived(state, code);
      });
      return util::response_from_string(request, IHttpRequest::Ok, {},
                                        MAKE_STRING(default_success_html));
    } else if (request.get("error")) {
      return util::response_from_string(request, IHttpRequest::Ok, {},
                                        MAKE_STRING(default_error_html));
    }
    if (request.url().find("/login") != std::string::npos) {
      std::string result;
      if (state == "4shared")
        result = MAKE_STRING(__4shared_login_html);
      else if (state == "amazons3")
        result = MAKE_STRING(amazons3_login_html);
      else if (state == "animezone")
        result = MAKE_STRING(animezone_login_html);
      else if (state == "local")
        result = MAKE_STRING(local_login_html);
      else if (state == "localwinrt")
        result = MAKE_STRING(localwinrt_login_html);
      else if (state == "mega")
        result = MAKE_STRING(mega_login_html);
      else if (state == "webdav")
        result = MAKE_STRING(webdav_login_html);
      else
        return util::response_from_string(request, IHttpRequest::NotFound, {},
                                          "Not found");
      return util::response_from_string(request, IHttpRequest::Ok, {}, result);
    }

    return util::response_from_string(request, IHttpRequest::Bad, {}, "");
  }

  CloudFactory* factory_;
};

struct FactoryCallbackWrapper : public ICloudFactory::ICallback {
  FactoryCallbackWrapper(CloudFactory* factory,
                         const std::shared_ptr<ICloudFactory::ICallback>& cb)
      : factory_(factory), cb_(cb) {}

  void onCloudAuthenticationCodeReceived(const std::string& provider,
                                         const std::string& code) override {
    if (cb_) cb_->onCloudAuthenticationCodeReceived(provider, code);
  }

  void onCloudTokenReceived(const std::string& provider,
                            const EitherError<Token>& token) override {
    if (cb_) cb_->onCloudTokenReceived(provider, token);
  }
  void onCloudCreated(const std::shared_ptr<ICloudAccess>& cloud) override {
    if (cb_) cb_->onCloudCreated(cloud);
  }
  void onCloudRemoved(const std::shared_ptr<ICloudAccess>& cloud) override {
    if (cb_) cb_->onCloudRemoved(cloud);
  }
  void onEventsAdded() override {
    factory_->onEventsAdded();
    if (cb_) cb_->onEventsAdded();
  }

  CloudFactory* factory_;
  std::shared_ptr<ICloudFactory::ICallback> cb_;
};

uint64_t cloud_identifier(const ICloudProvider& p) {
  auto state = p.hints()["state"];
  return std::stoull(state.substr(p.name().length() + 1));
}

}  // namespace

CloudFactory::CloudFactory(CloudFactory::InitData&& d)
    : callback_(std::make_shared<FactoryCallbackWrapper>(this, d.callback_)),
      event_loop_(callback_),
      base_url_(d.base_url_),
      http_(std::move(d.http_)),
      http_server_factory_(util::make_unique<ServerWrapperFactory>(
          d.http_server_factory_.get())),
      crypto_(std::move(d.crypto_)),
      thread_pool_(std::move(d.thread_pool_)),
      cloud_storage_(ICloudStorage::create()),
      provider_index_(),
      loop_(event_loop_.impl()) {
  for (const auto& d : cloud_storage_->providers()) {
    http_server_handles_.emplace_back(
        http_server_factory_->create(util::make_unique<HttpCallback>(this), d,
                                     IHttpServer::Type::Authorization));
  }
  http_server_handles_.push_back(
      http_server_factory_->create(util::make_unique<HttpCallback>(this),
                                   "static", IHttpServer::Type::FileProvider));
  http_server_handles_.push_back(http_server_factory_->create(
      util::make_unique<HttpCallback>(this), "favicon.ico",
      IHttpServer::Type::FileProvider));
}

CloudFactory::~CloudFactory() {
  loop_->clear();
  loop_->process_events();
  http_server_handles_.clear();
  cloud_access_.clear();
}

std::shared_ptr<ICloudAccess> CloudFactory::create(
    const std::string& provider_name,
    const ICloudFactory::ProviderInitData& data) {
  auto result = std::make_shared<CloudAccess>(createImpl(provider_name, data));
  cloud_access_.insert({cloud_identifier(*result->provider()), result});
  return std::static_pointer_cast<ICloudAccess>(result);
}

void CloudFactory::remove(const std::shared_ptr<ICloudAccess>& d) {
  for (auto it = cloud_access_.begin(); it != cloud_access_.end();) {
    if (it->second == d) {
      onCloudRemoved(it->second);
      it = cloud_access_.erase(it);
    } else {
      it++;
    }
  }
}

CloudAccess CloudFactory::createImpl(
    const std::string& provider_name,
    const CloudFactory::ProviderInitData& data) const {
  ICloudProvider::InitData init_data;
  init_data.token_ = std::move(data.token_);
  init_data.hints_ = std::move(data.hints_);
  init_data.permission_ = data.permission_;
  init_data.http_engine_ =
      http_ ? util::make_unique<HttpWrapper>(http_) : nullptr;
  init_data.http_server_ =
      http_server_factory_
          ? util::make_unique<HttpServerFactoryWrapper>(http_server_factory_)
          : nullptr;
  init_data.crypto_engine_ =
      crypto_ ? util::make_unique<CryptoWrapper>(crypto_) : nullptr;
  init_data.thread_pool_ =
      thread_pool_ ? util::make_unique<ThreadPoolWrapper>(thread_pool_)
                   : nullptr;
  init_data.callback_ =
      util::make_unique<AuthCallback>(const_cast<CloudFactory*>(this));
  auto index = provider_index_++;
  auto state = provider_name + "-" + std::to_string(index);
  init_data.hints_["state"] = state;
  init_data.hints_["file_url"] =
      base_url_ + (base_url_.back() == '/' ? "" : "/") + state;
  init_data.hints_["redirect_uri"] =
      base_url_ + (base_url_.back() == '/' ? "" : "/") + provider_name;
  if (config_["keys"].isMember(provider_name)) {
    init_data.hints_["client_id"] =
        config_["keys"][provider_name]["client_id"].asString();
    init_data.hints_["client_secret"] =
        config_["keys"][provider_name]["client_secret"].asString();
  }
  return CloudAccess(
      loop_, cloud_storage_->provider(provider_name, std::move(init_data)));
}

void CloudFactory::onEventsAdded() {
  std::unique_lock<std::mutex> lock(mutex_);
  events_ready_++;
  empty_condition_.notify_one();
}

void CloudFactory::add(std::unique_ptr<IGenericRequest>&& request) {
  loop_->add(loop_->next_tag(), std::move(request));
}

void CloudFactory::invoke(Function<void()>&& f) { loop_->invoke(std::move(f)); }

void CloudFactory::onCloudTokenReceived(const std::string& provider,
                                        const EitherError<Token>& token) {
  if (callback_) callback_->onCloudTokenReceived(provider, token);
  if (token.right()) {
    ProviderInitData init_data;
    init_data.token_ = token.right()->token_;
    init_data.hints_["access_token"] = token.right()->access_token_;
    auto cloud_access =
        std::make_shared<CloudAccess>(createImpl(provider, init_data));
    cloud_access_.insert(
        {cloud_identifier(*cloud_access->provider()), cloud_access});
    onCloudCreated(cloud_access);
  }
}

void CloudFactory::onCloudAuthenticationCodeReceived(
    const std::string& provider, const std::string& code) {
  if (callback_) callback_->onCloudAuthenticationCodeReceived(provider, code);
  CloudFactory::ProviderInitData data;
  data.permission_ = ICloudProvider::Permission::ReadWrite;
  auto access =
      std::make_shared<CloudAccess>(createImpl(provider, std::move(data)));
  add(access->provider()->exchangeCodeAsync(
      code, [factory = this, provider,
             access = std::move(access)](EitherError<Token> e) {
        factory->invoke([factory, provider, e, access = std::move(access)] {
          factory->onCloudTokenReceived(provider, e);
        });
      }));
}

void CloudFactory::onCloudCreated(std::shared_ptr<CloudAccess> d) {
  if (callback_) callback_->onCloudCreated(d);
}

void CloudFactory::onCloudRemoved(std::shared_ptr<CloudAccess> d) {
  if (callback_) callback_->onCloudRemoved(d);
}

std::string CloudFactory::authorizationUrl(const std::string& provider) const {
  ProviderInitData data;
  data.permission_ = ICloudProvider::Permission::ReadWrite;
  auto p = createImpl(provider, data);
  return p.provider()->authorizeLibraryUrl();
}

std::string CloudFactory::pretty(const std::string& provider) const {
  const std::unordered_map<std::string, std::string> name_map = {
      {"amazon", "Amazon Drive"},
      {"amazons3", "Amazon S3"},
      {"box", "Box"},
      {"dropbox", "Dropbox"},
      {"google", "Google Drive"},
      {"hubic", "hubiC"},
      {"mega", "Mega"},
      {"onedrive", "One Drive"},
      {"pcloud", "pCloud"},
      {"webdav", "WebDAV"},
      {"yandex", "Yandex Disk"},
      {"youtube", "YouTube"},
      {"gphotos", "Google Photos"},
      {"local", "Local Drive"},
      {"localwinrt", "Local Drive"},
      {"animezone", "Anime Zone"},
      {"4shared", "4shared"}};
  auto it = name_map.find(provider);
  if (it != name_map.end())
    return it->second.c_str();
  else
    return "";
}

std::vector<std::string> CloudFactory::availableProviders() const {
  return cloud_storage_->providers();
}

bool CloudFactory::httpServerAvailable() const {
  return http_server_factory_->serverAvailable();
}

void CloudFactory::onCloudRemoved(const ICloudProvider& d) {
  auto identifier = cloud_identifier(d);
  loop_->invoke([=] {
    auto it = cloud_access_.find(identifier);
    if (it != cloud_access_.end()) {
      onCloudRemoved(it->second);
      cloud_access_.erase(it);
    }
  });
}

bool CloudFactory::dumpAccounts(std::ostream&& stream) {
  try {
    Json::Value json;
    Json::Value providers;
    for (const auto& d : cloud_access_) {
      Json::Value provider;
      provider["type"] = d.second->provider()->name();
      provider["token"] = d.second->provider()->token();
      provider["access_token"] = d.second->provider()->hints()["access_token"];
      providers.append(provider);
    }
    json["providers"] = providers;
    stream << json;
    return true;
  } catch (const Json::Exception&) {
    return false;
  }
}

bool CloudFactory::dumpAccounts(std::ostream& stream) {
  return dumpAccounts(std::move(stream));
}

bool CloudFactory::loadAccounts(std::istream&& stream) {
  try {
    Json::Value json;
    stream >> json;
    for (const auto& d : json["providers"]) {
      ProviderInitData data;
      data.token_ = d["token"].asString();
      data.hints_["access_token"] = d["access_token"].asString();
      auto cloud =
          std::make_shared<CloudAccess>(createImpl(d["type"].asString(), data));
      onCloudCreated(cloud);
      cloud_access_.insert({cloud_identifier(*cloud->provider()), cloud});
    }
    return true;
  } catch (const Json::Exception&) {
    return false;
  }
}

bool CloudFactory::loadAccounts(std::istream& stream) {
  return loadAccounts(std::move(stream));
}

bool CloudFactory::loadConfig(std::istream&& stream) {
  try {
    stream >> config_;
    return true;
  } catch (const Json::Exception&) {
    return false;
  }
}

bool CloudFactory::loadConfig(std::istream& stream) {
  return loadConfig(std::move(stream));
}

void CloudFactory::processEvents() { event_loop_.processEvents(); }

int CloudFactory::exec() {
  std::unique_lock<std::mutex> lock(mutex_);
  static std::atomic_bool interrupt;
  interrupt = false;
  std::signal(SIGINT, [](int) { interrupt = true; });
  while (!quit_ && !interrupt) {
    empty_condition_.wait_for(lock, std::chrono::milliseconds(100), [=] {
      return quit_ || events_ready_ || interrupt;
    });
    if (events_ready_) {
      events_ready_--;
      lock.unlock();
      event_loop_.processEvents();
      lock.lock();
    }
  }
  return 0;
}

void CloudFactory::quit() {
  std::unique_lock<std::mutex> lock(mutex_);
  quit_ = true;
  empty_condition_.notify_one();
}

std::vector<std::shared_ptr<ICloudAccess>> CloudFactory::providers() const {
  std::vector<std::shared_ptr<ICloudAccess>> result;
  for (const auto& p : cloud_access_) {
    result.push_back(p.second);
  }
  return result;
}

std::unique_ptr<ICloudFactory> ICloudFactory::create(
    const ICloudFactory::ICallback::Pointer& callback) {
  ICloudFactory::InitData init_data;
  init_data.base_url_ = "http://localhost:12345";
  init_data.http_ = IHttp::create();
  init_data.http_server_factory_ = IHttpServerFactory::create();
  init_data.crypto_ = ICrypto::create();
  init_data.thread_pool_ = IThreadPool::create(1);
  init_data.callback_ = callback;
  return create(std::move(init_data));
}

std::unique_ptr<ICloudFactory> ICloudFactory::create(InitData&& d) {
  return util::make_unique<CloudFactory>(std::move(d));
}

}  // namespace cloudstorage
