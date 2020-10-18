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
  HttpWrapper(std::shared_ptr<IHttp> http) : http_(std::move(http)) {}

  IHttpRequest::Pointer create(const std::string& url,
                               const std::string& method,
                               bool follow_redirect) const override {
    return http_->create(url, method, follow_redirect);
  }

  std::shared_ptr<IHttp> http_;
};

struct CryptoWrapper : public ICrypto {
  CryptoWrapper(std::shared_ptr<ICrypto> crypto) : crypto_(std::move(crypto)) {}

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
  ThreadPoolWrapper(std::shared_ptr<IThreadPool> pool)
      : thread_pool_(std::move(pool)) {}

  void schedule(const Task& f,
                const std::chrono::system_clock::time_point& when) override {
    thread_pool_->schedule(f, when);
  }

  std::shared_ptr<IThreadPool> thread_pool_;
};

struct AuthCallback : public ICloudProvider::IAuthCallback {
  AuthCallback(CloudFactory* factory) : factory_(factory) {}

  Status userConsentRequired(const ICloudProvider&) override {
    return Status::None;
  }

  void done(const ICloudProvider&, EitherError<void> e) override {
    if (e.left()) {
      factory_->onCloudRemoved(access_);
    }
  }

  CloudFactory* factory_;
  ICloudAccess* access_ = nullptr;
};

struct HttpServerFactoryWrapper : public IHttpServerFactory {
  HttpServerFactoryWrapper(std::shared_ptr<IHttpServerFactory> factory)
      : factory_(std::move(factory)) {}

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
    auto code = util::get_authorization_code(request);
    if (code.has_value()) {
      factory_->invoke([factory = factory_, state, code = code.value()] {
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
                         std::shared_ptr<ICloudFactory::ICallback> cb)
      : factory_(factory), cb_(std::move(cb)) {}

  void onCloudAuthenticationCodeReceived(const std::string& provider,
                                         const std::string& code) override {
    if (cb_) cb_->onCloudAuthenticationCodeReceived(provider, code);
  }

  void onCloudAuthenticationCodeExchangeFailed(
      const std::string& provider, const IException& exception) override {
    if (cb_) cb_->onCloudAuthenticationCodeExchangeFailed(provider, exception);
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

}  // namespace

CloudFactory::CloudFactory(CloudFactory::InitData&& d)
    : callback_(std::make_shared<FactoryCallbackWrapper>(this, d.callback_)),
      event_loop_(util::make_unique<CloudEventLoop>(
          d.thread_pool_factory_.get(), callback_)),
      base_url_(d.base_url_),
      http_(std::move(d.http_)),
      http_server_factory_(util::make_unique<ServerWrapperFactory>(
          d.http_server_factory_.get())),
      crypto_(std::move(d.crypto_)),
      thread_pool_(d.thread_pool_factory_->create(1)),
      thread_pool_factory_(std::move(d.thread_pool_factory_)),
      cloud_storage_(ICloudStorage::create()),
      loop_(event_loop_->impl()) {
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
  http_server_factory_ = nullptr;
  crypto_ = nullptr;
  thread_pool_ = nullptr;
  thread_pool_factory_ = nullptr;
  http_ = nullptr;
  http_server_handles_.clear();
  loop_->clear();
  loop_ = nullptr;
  event_loop_ = nullptr;
  cloud_access_.clear();
}

std::shared_ptr<ICloudAccess> CloudFactory::create(
    const std::string& provider_name,
    const ICloudFactory::ProviderInitData& data) {
  auto result = std::shared_ptr<CloudAccess>(createImpl(provider_name, data));
  cloud_access_.insert(result);
  return std::static_pointer_cast<ICloudAccess>(result);
}

void CloudFactory::remove(const ICloudAccess& d) {
  auto tmp = std::shared_ptr<CloudAccess>(
      static_cast<CloudAccess*>(const_cast<ICloudAccess*>(&d)),
      [](ICloudAccess*) {});
  auto it = cloud_access_.find(tmp);
  if (it != cloud_access_.end()) {
    onCloudRemoved(*it);
    cloud_access_.erase(it);
  }
}

std::unique_ptr<CloudAccess> CloudFactory::createImpl(
    const std::string& provider_name,
    const CloudFactory::ProviderInitData& data) const {
  ICloudProvider::InitData init_data;
  init_data.token_ = data.token_;
  init_data.hints_ = data.hints_;
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
#ifdef WITH_THUMBNAILER
  init_data.thumbnailer_thread_pool =
      util::make_unique<ThreadPoolWrapper>(loop_->thumbnailer_thread_pool());
#endif
  init_data.callback_ =
      util::make_unique<AuthCallback>(const_cast<CloudFactory*>(this));
  auto auth_callback = static_cast<AuthCallback*>(init_data.callback_.get());
  auto state =
      std::to_string(reinterpret_cast<intptr_t>(init_data.callback_.get()));
  if (init_data.hints_.find("state") == init_data.hints_.end())
    init_data.hints_["state"] = state;
  if (init_data.hints_.find("file_url") == init_data.hints_.end())
    init_data.hints_["file_url"] =
        base_url_ + (base_url_.back() == '/' ? "" : "/") + state;
  if (init_data.hints_.find("redirect_uri") == init_data.hints_.end())
    init_data.hints_["redirect_uri"] =
        base_url_ + (base_url_.back() == '/' ? "" : "/") + provider_name;
  if (config_["keys"].isMember(provider_name)) {
    if (init_data.hints_.find("client_id") == init_data.hints_.end())
      init_data.hints_["client_id"] =
          config_["keys"][provider_name]["client_id"].asString();
    if (init_data.hints_.find("client_secret") == init_data.hints_.end())
      init_data.hints_["client_secret"] =
          config_["keys"][provider_name]["client_secret"].asString();
  }
  auto provider = cloud_storage_->provider(provider_name, std::move(init_data));
  if (!provider) return nullptr;
  auto result = util::make_unique<CloudAccess>(loop_, std::move(provider));
  auth_callback->access_ = result.get();
  return result;
}

void CloudFactory::onEventsAdded() {
  std::unique_lock<std::mutex> lock(mutex_);
  events_ready_++;
  empty_condition_.notify_one();
}

void CloudFactory::add(std::unique_ptr<IGenericRequest>&& request) {
  auto tag = reinterpret_cast<uintptr_t>(request.get());
  loop_->add(tag, std::move(request));
}

void CloudFactory::invoke(std::function<void()>&& f) {
  loop_->invoke(std::move(f));
}

void CloudFactory::onCloudTokenReceived(const std::string& provider,
                                        const EitherError<Token>& token) {
  if (callback_ && token.left())
    callback_->onCloudAuthenticationCodeExchangeFailed(provider,
                                                       Exception(token.left()));
  if (token.right()) {
    ProviderInitData init_data;
    init_data.token_ = token.right()->token_;
    init_data.hints_["access_token"] = token.right()->access_token_;
    auto cloud_access =
        std::shared_ptr<CloudAccess>(createImpl(provider, init_data));
    cloud_access_.insert(cloud_access);
    onCloudCreated(cloud_access);
  }
}

void CloudFactory::onCloudAuthenticationCodeReceived(
    const std::string& provider, const std::string& code) {
  if (callback_) callback_->onCloudAuthenticationCodeReceived(provider, code);
  CloudFactory::ProviderInitData data;
  data.permission_ = ICloudProvider::Permission::ReadWrite;
  auto access = std::shared_ptr<CloudAccess>(createImpl(provider, data));
  add(access->provider()->exchangeCodeAsync(
      code, [factory = this, provider, access](EitherError<Token> e) {
        factory->invoke([factory, provider, e, access] {
          factory->onCloudTokenReceived(provider, e);
        });
      }));
}

void CloudFactory::onCloudCreated(const std::shared_ptr<CloudAccess>& d) {
  if (callback_) callback_->onCloudCreated(d);
}

void CloudFactory::onCloudRemoved(const std::shared_ptr<CloudAccess>& d) {
  if (callback_) callback_->onCloudRemoved(d);
}

std::string CloudFactory::authorizationUrl(const std::string& provider,
                                           const ProviderInitData& data) const {
  auto p = createImpl(provider, data);
  return p->provider()->authorizeLibraryUrl();
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
      {"gphotos", "Google Photos"},
      {"local", "Local Drive"},
      {"localwinrt", "Local Drive"},
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

void CloudFactory::onCloudRemoved(ICloudAccess* d) {
  loop_->invoke([this, id = d] {
    auto tmp = std::shared_ptr<CloudAccess>(static_cast<CloudAccess*>(id),
                                            [](ICloudAccess*) {});
    auto it = cloud_access_.find(tmp);
    if (it != cloud_access_.end()) {
      onCloudRemoved(*it);
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
      provider["type"] = d->provider()->name();
      provider["token"] = d->provider()->token();
      provider["access_token"] = d->provider()->hints()["access_token"];
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
          std::shared_ptr<CloudAccess>(createImpl(d["type"].asString(), data));
      onCloudCreated(cloud);
      cloud_access_.insert(cloud);
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

void CloudFactory::processEvents() { event_loop_->processEvents(); }

int CloudFactory::exec() {
  std::unique_lock<std::mutex> lock(mutex_);
  static std::atomic_bool interrupt;
  interrupt = false;
  std::signal(SIGINT, [](int) { interrupt = true; });
  while (!quit_ && !interrupt) {
    empty_condition_.wait_for(lock, std::chrono::milliseconds(100), [this] {
      return quit_ || events_ready_ || interrupt;
    });
    if (events_ready_) {
      events_ready_--;
      lock.unlock();
      event_loop_->processEvents();
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
  return std::vector<std::shared_ptr<ICloudAccess>>(cloud_access_.begin(),
                                                    cloud_access_.end());
}

Promise<Token> CloudFactory::exchangeAuthorizationCode(
    const std::string& provider, const ProviderInitData& data,
    const std::string& code) {
  Promise<Token> result;
  auto access = std::shared_ptr<CloudAccess>(createImpl(provider, data));
  add(access->provider()->exchangeCodeAsync(
      code, [factory = this, provider, result, access](EitherError<Token> e) {
        factory->invoke([provider, result, e, access] {
          if (e.left())
            result.reject(Exception(e.left()->code_, e.left()->description_));
          else
            result.fulfill(std::move(*e.right()));
        });
      }));
  return result;
}

std::unique_ptr<ICloudFactory> ICloudFactory::create(
    const ICloudFactory::ICallback::Pointer& callback) {
  ICloudFactory::InitData init_data;
  init_data.base_url_ = "http://localhost:12345";
  init_data.http_ = IHttp::create();
  init_data.http_server_factory_ = IHttpServerFactory::create();
  init_data.crypto_ = ICrypto::create();
  struct ThreadPoolFactory : public IThreadPoolFactory {
    IThreadPool::Pointer create(uint32_t size) override {
      return IThreadPool::create(size);
    }
  };
  init_data.thread_pool_factory_ = util::make_unique<ThreadPoolFactory>();
  init_data.callback_ = callback;
  return create(std::move(init_data));
}

std::unique_ptr<ICloudFactory> ICloudFactory::create(InitData&& d) {
  return util::make_unique<CloudFactory>(std::move(d));
}

void ICloudFactory::initialize(void* javaVM) {
  ICloudStorage::initialize(javaVM);
}

}  // namespace cloudstorage
