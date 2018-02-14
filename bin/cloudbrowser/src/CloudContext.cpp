#include "CloudContext.h"

#include <QCursor>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlEngine>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>
#include <unordered_set>
#include "File.h"
#include "GenerateThumbnail.h"
#include "ICloudStorage.h"
#include "Utility/Utility.h"

using namespace cloudstorage;

int ProviderListModel::rowCount(const QModelIndex&) const {
  return provider_.size();
}

QVariant ProviderListModel::data(const QModelIndex& index, int) const {
  return provider_[index.row()].variant();
}

QHash<int, QByteArray> ProviderListModel::roleNames() const {
  return {{Qt::DisplayRole, "modelData"}};
}

void ProviderListModel::remove(QVariant provider) {
  auto label = provider.toMap()["label"].toString().toStdString();
  auto type = provider.toMap()["type"].toString().toStdString();
  for (size_t i = 0; i < provider_.size();)
    if (provider_[i].label_ == label &&
        provider_[i].provider_->name() == type) {
      beginRemoveRows(QModelIndex(), i, i);
      provider_.erase(provider_.begin() + i);
      endRemoveRows();
      emit updated();
    } else {
      i++;
    }
}

Provider ProviderListModel::provider(QVariant provider) const {
  auto label = provider.toMap()["label"].toString().toStdString();
  auto type = provider.toMap()["type"].toString().toStdString();
  for (auto&& i : provider_)
    if (i.label_ == label && i.provider_->name() == type) return i;
  return {};
}

QVariantList ProviderListModel::dump() const {
  QVariantList array;
  for (auto&& p : provider_) {
    QVariantMap dict;
    dict["token"] = p.provider_->token().c_str();
    dict["access_token"] = p.provider_->hints()["access_token"].c_str();
    dict["type"] = p.provider_->name().c_str();
    dict["label"] = p.label_.c_str();
    array.append(dict);
  }
  return array;
}

void ProviderListModel::add(const Provider& p) {
  std::unordered_set<std::string> names;
  for (auto&& i : provider_)
    if (i.provider_->name() == p.provider_->name()) names.insert(i.label_);
  if (names.find(p.label_) == names.end()) {
    beginInsertRows(QModelIndex(), rowCount(), rowCount());
    provider_.push_back(p);
    endInsertRows();
    emit updated();
  }
}

QVariantList ProviderListModel::variant() const {
  QVariantList result;
  for (auto&& p : provider_) result.push_back(p.variant());
  return result;
}

CloudContext::CloudContext(QObject* parent)
    : QObject(parent),
      config_([]() {
        QFile file(":/config.json");
        file.open(QFile::ReadOnly);
        return QJsonDocument::fromJson(file.readAll());
      }()),
      http_server_factory_(util::make_unique<ServerWrapperFactory>(
          IHttpServerFactory::create())),
      http_(IHttp::create()),
      thread_pool_(IThreadPool::create(1)),
      cache_size_(updatedCacheSize()) {
  util::log_stream(util::make_unique<std::ostream>(&debug_stream_));
  std::lock_guard<std::mutex> lock(mutex_);
  QSettings settings;
  auto providers = settings.value("providers").toList();
  for (auto j : providers) {
    auto obj = j.toMap();
    auto label = obj["label"].toString().toStdString();
    user_provider_model_.add(
        {label,
         this->provider(obj["type"].toString().toStdString(), label,
                        Token{obj["token"].toString().toStdString(),
                              obj["access_token"].toString().toStdString()})});
  }
  for (auto p : ICloudStorage::create()->providers()) {
    auth_server_.push_back(http_server_factory_->create(
        util::make_unique<HttpServerCallback>(this), p,
        IHttpServer::Type::Authorization));
  }
  connect(this, &CloudContext::errorOccurred, this,
          [](QString operation, QVariantMap provider, int code,
             QString description) {
            util::log("(" + provider["type"].toString().toStdString() + ", " +
                          provider["label"].toString().toStdString() + ")",
                      operation.toStdString() + ":", code,
                      description.toStdString());
          });
  loadCachedDirectories();
}

CloudContext::~CloudContext() {
  save();
  util::log_stream(util::make_unique<std::ostream>(std::cerr.rdbuf()));
}

void CloudContext::loadCachedDirectories() {
  QFile file(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) +
             "/cloudstorage_cache.json");
  if (file.open(QFile::ReadOnly)) {
    QJsonObject json = QJsonDocument::fromBinaryData(file.readAll()).object();
    for (auto directory : json["directory"].toArray()) {
      auto json = directory.toObject();
      IItem::List items;
      for (auto item : json["list"].toArray()) {
        try {
          items.push_back(IItem::fromString(item.toString().toStdString()));
        } catch (const std::exception& e) {
          qDebug() << e.what();
        }
      }
      list_directory_cache_[{json["type"].toString().toStdString(),
                             json["label"].toString().toStdString(),
                             json["id"].toString().toStdString()}] = items;
    }
  }
}

void CloudContext::saveCachedDirectories() {
  QJsonArray cache;
  for (auto&& d : list_directory_cache_) {
    QJsonObject object;
    QJsonArray array;
    for (auto&& d : d.second) {
      array.append(d->toString().c_str());
    }
    object["type"] = d.first.provider_type_.c_str();
    object["label"] = d.first.provider_label_.c_str();
    object["id"] = d.first.directory_id_.c_str();
    object["list"] = array;
    cache.append(object);
  }
  QJsonObject json;
  json["directory"] = cache;
  QFile file(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) +
             "/cloudstorage_cache.json");
  if (file.open(QFile::WriteOnly))
    file.write(QJsonDocument(json).toBinaryData());
}

void CloudContext::save() {
  std::lock_guard<std::mutex> lock(mutex_);
  QSettings settings;
  settings.setValue("providers", user_provider_model_.dump());
  saveCachedDirectories();
}

QStringList CloudContext::providers() const {
  QStringList list;
  for (auto&& p : ICloudStorage::create()->providers()) list.append(p.c_str());
  return list;
}

ProviderListModel* CloudContext::userProviders() {
  return &user_provider_model_;
}

bool CloudContext::includeAds() const {
  return config_.object()["include_ads"].toBool();
}

bool CloudContext::isFree() const {
  return config_.object()["is_free"].toBool();
}

bool CloudContext::httpServerAvailable() const {
  return http_server_factory_->serverAvailable();
}

QString CloudContext::authorizationUrl(QString provider) const {
  auto data = init_data(provider.toStdString());
  return ICloudStorage::create()
      ->provider(provider.toStdString().c_str(), std::move(data))
      ->authorizeLibraryUrl()
      .c_str();
}

QObject* CloudContext::root(QVariant provider) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto p = user_provider_model_.provider(provider);
  if (p.provider_) {
    auto item = new CloudItem(p, p.provider_->rootDirectory());
    QQmlEngine::setObjectOwnership(item, QQmlEngine::JavaScriptOwnership);
    return item;
  } else {
    return nullptr;
  }
}

void CloudContext::removeProvider(QVariant provider) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    user_provider_model_.remove(provider);
  }
  save();
}

QString CloudContext::pretty(QString provider) const {
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
      {"local", "Local Drive"}};
  auto it = name_map.find(provider.toStdString());
  if (it != name_map.end())
    return it->second.c_str();
  else
    return "";
}

QVariantMap CloudContext::readUrl(QString url) const {
  util::Url result(url.toStdString());
  QVariantMap r;
  r["protocol"] = result.protocol().c_str();
  r["host"] = result.host().c_str();
  for (auto str : QString(result.query().c_str()).split('&')) {
    auto lst = str.split('=');
    if (lst.size() == 2)
      r[lst[0]] = util::Url::unescape(lst[1].toStdString()).c_str();
  }
  return r;
}

QString CloudContext::home() const {
  return QUrl::fromLocalFile(util::home_directory().c_str()).toString();
}

void CloudContext::hideCursor() const {
  QGuiApplication::setOverrideCursor(QCursor(Qt::BlankCursor));
}

QString CloudContext::supportUrl(QString name) const {
  return config_.object()["support_url"].toObject()[name].toString();
}

qint64 CloudContext::cacheSize() const { return cache_size_; }

void CloudContext::clearCache() {
  QString path =
      QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
  QDir dir(path);
  for (auto&& d : dir.entryList()) {
    if (d.endsWith("-thumbnail") || d == "cloudstorage_cache.json")
      QFile(path + "/" + d).remove();
  }

  cache_size_ = 0;
  emit cacheSizeChanged();

  std::lock_guard<std::mutex> lock(mutex_);
  list_directory_cache_.clear();
}

void CloudContext::addCacheSize(size_t size) {
  cache_size_ += size;
  emit cacheSizeChanged();
}

qint64 CloudContext::updatedCacheSize() const {
  QString path =
      QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
  QDir dir(path);
  qint64 result = 0;
  for (auto&& d : dir.entryList()) {
    if (d.endsWith("-thumbnail") || d == "cloudstorage_cache.json")
      result += QFile(path + "/" + d).size();
  }
  return result;
}

void CloudContext::showCursor() const {
  QGuiApplication::setOverrideCursor(QCursor(Qt::ArrowCursor));
}

void CloudContext::add(std::shared_ptr<ICloudProvider> p,
                       std::shared_ptr<IGenericRequest> r) {
  pool_.add(p, r);
}

void CloudContext::cacheDirectory(ListDirectoryCacheKey directory,
                                  const IItem::List& lst) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> data;
    list_directory_cache_[directory] = lst;
  }
  schedule([=] {
    std::lock_guard<std::mutex> lock(mutex_);
    saveCachedDirectories();
  });
}

IItem::List CloudContext::cachedDirectory(ListDirectoryCacheKey key) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = list_directory_cache_.find(key);
  if (it == std::end(list_directory_cache_))
    return {};
  else
    return it->second;
}

void CloudContext::schedule(std::function<void()> f) {
  thread_pool_->schedule(f);
}

void CloudContext::receivedCode(std::string provider, std::string code) {
  auto p = ICloudStorage::create()->provider(provider, init_data(provider));
  auto r = p->exchangeCodeAsync(code, [=](EitherError<Token> e) {
    QVariantMap provider_variant{{"type", provider.c_str()},
                                 {"label", pretty(provider.c_str())}};
    if (e.left())
      return emit errorOccurred("ExchangeCode", provider_variant,
                                e.left()->code_,
                                e.left()->description_.c_str());
    std::shared_ptr<ICloudProvider> p =
        this->provider(provider, "####", *e.right());
    pool_.add(p, p->getGeneralDataAsync([=](EitherError<GeneralData> d) {
      if (d.left())
        return emit errorOccurred("GeneralData", provider_variant,
                                  d.left()->code_,
                                  d.left()->description_.c_str());
      {
        std::unique_lock<std::mutex> lock(mutex_);
        user_provider_model_.add(
            {d.right()->username_,
             this->provider(provider, d.right()->username_, *e.right())});
      }
      save();
    }));
  });
  pool_.add(std::move(p), std::move(r));
  emit receivedCode(provider.c_str());
}

ICloudProvider::Pointer CloudContext::provider(const std::string& name,
                                               const std::string& label,
                                               Token token) const {
  class HttpWrapper : public IHttp {
   public:
    HttpWrapper(std::shared_ptr<IHttp> http) : http_(http) {}

    IHttpRequest::Pointer create(const std::string& url,
                                 const std::string& method,
                                 bool follow_redirect) const override {
      return http_->create(url, method, follow_redirect);
    }

   private:
    std::shared_ptr<IHttp> http_;
  };
  class HttpServerFactoryWrapper : public IHttpServerFactory {
   public:
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
  class ThreadPoolWrapper : public IThreadPool {
   public:
    ThreadPoolWrapper(std::shared_ptr<IThreadPool> thread_pool)
        : thread_pool_(thread_pool) {}

    void schedule(std::function<void()> f) override {
      thread_pool_->schedule(f);
    }

   private:
    std::shared_ptr<IThreadPool> thread_pool_;
  };
  class AuthCallback : public ICloudProvider::IAuthCallback {
   public:
    Status userConsentRequired(const ICloudProvider&) override {
      return Status::None;
    }

    void done(const ICloudProvider&, EitherError<void>) override {}
  };
  auto data = init_data(name);
  data.token_ = token.token_;
  data.hints_["temporary_directory"] =
      QDir::toNativeSeparators(QDir::tempPath() + "/").toStdString();
  data.hints_["access_token"] = token.access_token_;
  data.hints_["file_url"] = "http://localhost:12345/" + util::to_base64(label);
  data.hints_["state"] = util::to_base64(label);
  data.http_engine_ = util::make_unique<HttpWrapper>(http_);
  data.http_server_ =
      util::make_unique<HttpServerFactoryWrapper>(http_server_factory_);
  data.thread_pool_ = util::make_unique<ThreadPoolWrapper>(thread_pool_);
  data.callback_ = util::make_unique<AuthCallback>();
  return ICloudStorage::create()->provider(name, std::move(data));
}

ICloudProvider::InitData CloudContext::init_data(
    const std::string& name) const {
  ICloudProvider::InitData data;
  data.permission_ = ICloudProvider::Permission::ReadWrite;
  data.hints_["redirect_uri"] = "http://localhost:12345/" + name;
  data.hints_["client_id"] = config_.object()["keys"]
                                 .toObject()[name.c_str()]
                                 .toObject()["client_id"]
                                 .toString()
                                 .toStdString();
  data.hints_["client_secret"] = config_.object()["keys"]
                                     .toObject()[name.c_str()]
                                     .toObject()["client_secret"]
                                     .toString()
                                     .toStdString();
  return data;
}

CloudContext::HttpServerCallback::HttpServerCallback(CloudContext* ctx)
    : ctx_(ctx) {}

IHttpServer::IResponse::Pointer CloudContext::HttpServerCallback::handle(
    const IHttpServer::IRequest& request) {
  auto state = first_url_part(request.url());
  auto code = request.get("code");
  if (code) {
    QFile file(":/resources/default_success.html");
    file.open(QFile::ReadOnly);
    ctx_->receivedCode(state, code);
    return util::response_from_string(request, IHttpRequest::Ok, {},
                                      file.readAll().constData());
  } else if (QString(request.url().c_str()).endsWith("/login")) {
    QFile file(":/resources/" + QString(state.c_str()) + "_login.html");
    file.open(QFile::ReadOnly);
    return util::response_from_string(request, IHttpRequest::Ok, {},
                                      file.readAll().constData());
  } else {
    std::string message = "error occurred\n";
    if (!code) message += "code parameter is missing\n";
    if (request.get("error"))
      message += std::string(request.get("error")) + "\n";
    if (request.get("error_description"))
      message += std::string(request.get("error_description")) + "\n";
    return util::response_from_string(request, IHttpRequest::Bad, {}, message);
  }
}

CloudContext::RequestPool::RequestPool()
    : done_(), cleanup_thread_(std::async(std::launch::async, [=] {
        while (!done_) {
          std::unique_lock<std::mutex> lock(mutex_);
          condition_.wait(lock, [=] { return done_ || !request_.empty(); });
          while (!request_.empty()) {
            {
              auto r = request_.front();
              request_.pop_front();
              current_request_ = r.request_;
              lock.unlock();
              if (!done_)
                r.request_->finish();
              else
                r.request_->cancel();
            }
            lock.lock();
          }
        }
      })) {}

CloudContext::RequestPool::~RequestPool() {
  done_ = true;
  condition_.notify_one();
  std::unique_lock<std::mutex> lock(mutex_);
  if (current_request_) current_request_->cancel();
}

void CloudContext::RequestPool::add(std::shared_ptr<ICloudProvider> p,
                                    std::shared_ptr<IGenericRequest> r) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    request_.push_back({p, r});
  }
  condition_.notify_one();
}

std::streambuf::int_type CloudContext::DebugStream::overflow(int_type ch) {
  if (ch == '\n' || current_line_.length() > 100) {
    qDebug() << current_line_;
    current_line_ = "";
  } else {
    current_line_ += static_cast<char>(ch);
  }
  return 1;
}
