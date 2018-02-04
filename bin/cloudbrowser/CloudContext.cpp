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

#undef CreateDirectory

using namespace cloudstorage;

namespace {

QString sanitize(const QString& name) {
  const QString forbidden = "~\"#%&*:<>?/\\{|}";
  QString result;
  for (auto&& c : name)
    if (forbidden.indexOf(c) == -1)
      result += c;
    else
      result += '_';
  return result;
}

std::string first_url_part(const std::string& url) {
  auto idx = url.find_first_of('/', 1);
  if (idx == std::string::npos)
    return url.substr(1);
  else
    return std::string(url.begin() + 1, url.begin() + idx);
}

class DownloadToString : public IDownloadFileCallback {
 public:
  void receivedData(const char* data, uint32_t length) override {
    data_ += std::string(data, length);
  }

  void progress(uint64_t, uint64_t) override {}

  void done(EitherError<void>) override {}

  const std::string& data() const { return data_; }

 private:
  std::string data_;
};

class DownloadCallback : public IDownloadFileCallback {
 public:
  DownloadCallback(RequestNotifier* notifier, QUrl path)
      : notifier_(notifier), file_(path.toString()) {
    file_.open(QFile::WriteOnly);
  }

  void receivedData(const char* data, uint32_t length) override {
    file_.write(data, static_cast<qint64>(length));
  }

  void progress(uint64_t total, uint64_t now) override {
    emit notifier_->progressChanged(static_cast<qint64>(total),
                                    static_cast<qint64>(now));
  }

  void done(EitherError<void> e) override {
    file_.close();
    emit notifier_->finishedVoid(e);
    emit notifier_->progressChanged(0, 0);
    notifier_->deleteLater();
  }

 protected:
  RequestNotifier* notifier_;
  QString name_;
  File file_;
};

}  // namespace

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
    provider_.push_back(
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
      std::vector<IItem::Pointer> items;
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
  QVariantList array;
  for (auto&& p : provider_) {
    QVariantMap dict;
    dict["token"] = p.provider_->token().c_str();
    dict["access_token"] = p.provider_->hints()["access_token"].c_str();
    dict["type"] = p.provider_->name().c_str();
    dict["label"] = p.label_.c_str();
    array.append(dict);
  }
  settings.setValue("providers", array);
  saveCachedDirectories();
}

QStringList CloudContext::providers() const {
  QStringList list;
  for (auto&& p : ICloudStorage::create()->providers()) list.append(p.c_str());
  return list;
}

QVariantList CloudContext::userProviders() const {
  std::lock_guard<std::mutex> lock(mutex_);
  QVariantList result;
  for (auto&& i : provider_) result.push_back(i.variant());
  return result;
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
  auto label = provider.toMap()["label"].toString().toStdString();
  auto type = provider.toMap()["type"].toString().toStdString();
  for (auto&& i : provider_)
    if (i.label_ == label && i.provider_->name() == type) {
      auto item = new CloudItem(i, i.provider_->rootDirectory());
      QQmlEngine::setObjectOwnership(item, QQmlEngine::JavaScriptOwnership);
      return item;
    }
  return nullptr;
}

void CloudContext::removeProvider(QVariant provider) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto label = provider.toMap()["label"].toString().toStdString();
    auto type = provider.toMap()["type"].toString().toStdString();
    std::vector<Provider> r;
    for (auto&& i : provider_)
      if (i.label_ != label || i.provider_->name() != type) r.push_back(i);
    provider_ = r;
  }
  emit userProvidersChanged();
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

void CloudContext::cacheDirectory(CloudItem* directory,
                                  const std::vector<IItem::Pointer>& lst) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> data;
    list_directory_cache_[{directory->provider().provider_->name(),
                           directory->provider().label_,
                           directory->item()->id()}] = lst;
  }
  schedule([=] {
    std::lock_guard<std::mutex> lock(mutex_);
    saveCachedDirectories();
  });
}

std::vector<IItem::Pointer> CloudContext::cachedDirectory(
    CloudItem* directory) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = list_directory_cache_.find({directory->provider().provider_->name(),
                                        directory->provider().label_,
                                        directory->item()->id()});
  if (it == std::end(list_directory_cache_))
    return {};
  else
    return it->second;
}

void CloudContext::schedule(std::function<void()> f) {
  thread_pool_->schedule(f);
}

QString CloudContext::thumbnail_path(const QString& filename) {
  return QStandardPaths::writableLocation(QStandardPaths::CacheLocation) +
         QDir::separator() + sanitize(filename) + "-thumbnail";
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
      std::unique_lock<std::mutex> lock(mutex_);
      std::unordered_set<std::string> names;
      for (auto&& i : provider_)
        if (i.provider_->name() == provider) names.insert(i.label_);
      if (names.find(d.right()->username_) == names.end()) {
        provider_.push_back(
            {d.right()->username_,
             this->provider(provider, d.right()->username_, *e.right())});
        lock.unlock();
        save();
        emit userProvidersChanged();
      }
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

ICloudProvider* CloudContext::provider(const std::string& label) const {
  std::lock_guard<std::mutex> lock(mutex_);
  for (auto&& i : provider_)
    if (i.label_ == label) return i.provider_.get();
  return nullptr;
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

CloudItem::CloudItem(const Provider& p, IItem::Pointer item, QObject* parent)
    : QObject(parent), provider_(p), item_(item) {}

QString CloudItem::filename() const { return item_->filename().c_str(); }

QString CloudItem::type() const {
  switch (item_->type()) {
    case IItem::FileType::Unknown:
      return "file";
    case IItem::FileType::Audio:
      return "audio";
    case IItem::FileType::Image:
      return "image";
    case IItem::FileType::Video:
      return "video";
    case IItem::FileType::Directory:
      return "directory";
    default:
      return "";
  }
}

bool CloudItem::supports(QString operation) const {
  auto provider = this->provider().provider_;
  if (operation == "delete")
    return provider->supportedOperations() & ICloudProvider::DeleteItem;
  else if (operation == "upload")
    return provider->supportedOperations() & ICloudProvider::UploadFile;
  else if (operation == "move")
    return provider->supportedOperations() & ICloudProvider::MoveItem;
  else if (operation == "rename")
    return provider->supportedOperations() & ICloudProvider::RenameItem;
  else if (operation == "mkdir")
    return provider->supportedOperations() & ICloudProvider::CreateDirectory;
  else if (operation == "download")
    return provider->supportedOperations() & ICloudProvider::DownloadFile;
  else
    return true;
}

void ListDirectoryModel::set_provider(const Provider& p) { provider_ = p; }

void ListDirectoryModel::add(IItem::Pointer t) {
  beginInsertRows(QModelIndex(), rowCount(), rowCount());
  list_.push_back(t);
  id_.insert(t->id());
  endInsertRows();
}

void ListDirectoryModel::clear() {
  beginRemoveRows(QModelIndex(), 0, std::max<int>(rowCount() - 1, 0));
  list_.clear();
  id_.clear();
  endRemoveRows();
}

int ListDirectoryModel::find(IItem::Pointer item) const {
  if (id_.find(item->id()) == std::end(id_)) return -1;
  for (size_t i = 0; i < list_.size(); i++)
    if (list_[i]->id() == item->id()) return i;
  return -1;
}

void ListDirectoryModel::insert(int idx, IItem::Pointer item) {
  beginInsertRows(QModelIndex(), idx, idx);
  list_.insert(list_.begin() + idx, item);
  id_.insert(item->id());
  endInsertRows();
}

void ListDirectoryModel::remove(int idx) {
  beginRemoveRows(QModelIndex(), idx, idx);
  id_.erase(id_.find(list_[idx]->id()));
  list_.erase(list_.begin() + idx);
  endRemoveRows();
}

void ListDirectoryModel::match(const std::vector<IItem::Pointer>& lst) {
  std::unordered_set<std::string> id;
  for (size_t i = lst.size(); i-- > 0;) {
    auto idx = find(lst[i]);
    if (idx == -1) {
      beginInsertRows(QModelIndex(), 0, 0);
      list_.insert(list_.begin(), lst[i]);
      id_.insert(lst[i]->id());
      endInsertRows();
    } else {
      list_[idx] = lst[i];
      emit dataChanged(createIndex(idx, 0), createIndex(idx, 0));
    }
    id.insert(lst[i]->id());
  }
  for (size_t i = 0; i < list().size();) {
    if (id.find(list()[i]->id()) == std::end(id)) {
      remove(i);
    } else {
      i++;
    }
  }
}

int ListDirectoryModel::rowCount(const QModelIndex&) const {
  return static_cast<int>(list_.size());
}

QVariant ListDirectoryModel::data(const QModelIndex& id, int) const {
  if (static_cast<uint32_t>(id.row()) >= list_.size()) return QVariant();
  auto item = new CloudItem(provider_, list_[static_cast<size_t>(id.row())]);
  QQmlEngine::setObjectOwnership(item, QQmlEngine::JavaScriptOwnership);
  return QVariant::fromValue(item);
}

QHash<int, QByteArray> ListDirectoryModel::roleNames() const {
  return {{Qt::DisplayRole, "modelData"}};
}

ListDirectoryModel* ListDirectoryRequest::list() { return &list_; }

void ListDirectoryRequest::update(CloudContext* context, CloudItem* item) {
  list_.set_provider(item->provider());

  auto cached = context->cachedDirectory(item);
  bool cache_found = !cached.empty();
  if (!cached.empty()) {
    list_.match(cached);
  }
  set_done(false);

  auto object = new RequestNotifier;
  auto provider = item->provider().variant();
  connect(object, &RequestNotifier::finishedList, this,
          [=](EitherError<std::vector<IItem::Pointer>> r) {
            set_done(true);
            if (r.left())
              return emit context->errorOccurred(
                  "ListDirectory", provider, r.left()->code_,
                  r.left()->description_.c_str());
            context->cacheDirectory(item, *r.right());
            if (cache_found) {
              list_.match(*r.right());
            }
          });
  connect(object, &RequestNotifier::addedItem, this, [=](IItem::Pointer item) {
    if (!cache_found) list_.add(item);
  });
  class ListDirectoryCallback : public IListDirectoryCallback {
   public:
    ListDirectoryCallback(RequestNotifier* r) : notifier_(r) {}

    void receivedItem(IItem::Pointer item) override {
      notifier_->addedItem(item);
    }

    void done(EitherError<std::vector<IItem::Pointer>> r) override {
      emit notifier_->finishedList(r);
      notifier_->deleteLater();
    }

   private:
    RequestNotifier* notifier_;
  };
  auto r = item->provider().provider_->listDirectoryAsync(
      item->item(), util::make_unique<ListDirectoryCallback>(object));
  context->add(item->provider().provider_, std::move(r));
}

void Request::set_done(bool done) {
  if (done_ == done) return;
  done_ = done;
  emit doneChanged();
}

void GetThumbnailRequest::update(CloudContext* context, CloudItem* item) {
  if (item->type() == "directory") return set_done(true);
  set_done(false);
  auto path = CloudContext::thumbnail_path(item->filename());
  QFile file(path);
  if (file.exists() && file.size() > 0) {
    source_ = QUrl::fromLocalFile(path).toString();
    set_done(true);
    emit sourceChanged();
  } else {
    auto object = new RequestNotifier;
    auto provider = item->provider().variant();
    connect(object, &RequestNotifier::finishedString, this,
            [=](EitherError<std::string> e) {
              set_done(true);
              if (e.left())
                return emit context->errorOccurred(
                    "GetThumbnail", provider, e.left()->code_,
                    e.left()->description_.c_str());
              source_ = QUrl::fromLocalFile(e.right()->c_str()).toString();
              emit sourceChanged();
            });
    class DownloadThumbnailCallback : public IDownloadFileCallback {
     public:
      DownloadThumbnailCallback(CloudContext* context,
                                RequestNotifier* notifier,
                                std::shared_ptr<ICloudProvider> p,
                                IItem::Pointer item)
          : context_(context), notifier_(notifier), provider_(p), item_(item) {}

      void receivedData(const char* data, uint32_t length) override {
        data_ += std::string(data, length);
      }

      void progress(uint64_t total, uint64_t now) override {
        emit notifier_->progressChanged(static_cast<qint64>(total),
                                        static_cast<qint64>(now));
      }

      static void submit(RequestNotifier* notifier, CloudContext* context,
                         QString path, const std::string& data) {
        auto e = save(path, data);
        if (e.left()) {
          notifier->finishedString(e.left());
        } else {
          context->addCacheSize(data.size());
          notifier->finishedString(*e.right());
        }
        notifier->deleteLater();
      }

      static EitherError<std::string> save(QString path,
                                           const std::string& data) {
        QSaveFile file(path);
        if (!file.open(QFile::WriteOnly))
          return Error{IHttpRequest::Bad, "couldn't open file"};
        file.write(data.c_str(), static_cast<qint64>(data.size()));
        if (file.commit()) {
          return path.toStdString();
        } else {
          return Error{IHttpRequest::Bad, "couldn't commit file"};
        }
      }

      void done(EitherError<void> e) override {
#ifdef WITH_THUMBNAILER
        if (e.left() && (item_->type() == IItem::FileType::Image ||
                         item_->type() == IItem::FileType::Video)) {
          auto path = CloudContext::thumbnail_path(item_->filename().c_str());
          auto context = context_;
          auto provider = provider_;
          auto item = item_;
          auto notifier = notifier_;
          std::thread([path, context, provider, item, notifier] {
            auto r = provider->getItemUrlAsync(item)->result();
            if (r.left()) {
              emit notifier->finishedString(r.left());
              return notifier->deleteLater();
            }
            if (item->type() == IItem::FileType::Image) {
              if (item->size() < 2 * 1024 * 1024) {
                auto downloader = std::make_shared<DownloadToString>();
                auto e =
                    provider->downloadFileAsync(item, downloader)->result();
                if (e.left())
                  emit notifier->finishedString(e.left());
                else
                  return submit(notifier, context, path, downloader->data());
              } else {
                emit notifier->finishedString(
                    Error{IHttpRequest::ServiceUnavailable, "image too big"});
              }
              return notifier->deleteLater();
            }
            auto e = generate_thumbnail(*r.right());
            if (e.left()) {
              emit notifier->finishedString(e.left());
              return notifier->deleteLater();
            }
            submit(notifier, context, path, *e.right());
          }).detach();
          return;
        }
#endif
        if (e.left())
          emit notifier_->finishedString(e.left());
        else {
          auto context = context_;
          auto notifier = notifier_;
          auto path = CloudContext::thumbnail_path(item_->filename().c_str());
          auto data = std::move(data_);
          context_->schedule([context, notifier, path, data] {
            submit(notifier, context, path, std::move(data));
          });
        }
      }

     private:
      CloudContext* context_;
      RequestNotifier* notifier_;
      std::string data_;
      std::shared_ptr<ICloudProvider> provider_;
      IItem::Pointer item_;
    };
    auto p = item->provider().provider_;
    auto r = p->getThumbnailAsync(item->item(),
                                  util::make_unique<DownloadThumbnailCallback>(
                                      context, object, p, item->item()));
    context->add(p, std::move(r));
  }
}

void GetUrlRequest::update(CloudContext* context, CloudItem* item) {
  set_done(false);
  auto object = new RequestNotifier;
  auto provider = item->provider().variant();
  connect(object, &RequestNotifier::finishedString, this,
          [=](EitherError<std::string> e) {
            if (e.left()) {
              source_ = "";
              emit context->errorOccurred("GetItemUrl", provider,
                                          e.left()->code_,
                                          e.left()->description_.c_str());
            } else
              source_ = e.right()->c_str();
            emit sourceChanged();
            set_done(true);
          });
  auto p = item->provider().provider_;
  auto r =
      p->getItemUrlAsync(item->item(), [object](EitherError<std::string> e) {
        emit object->finishedString(e);
        object->deleteLater();
      });
  context->add(p, std::move(r));
}

void CreateDirectoryRequest::update(CloudContext* context, CloudItem* parent,
                                    QString name) {
  set_done(false);
  auto object = new RequestNotifier;
  auto provider = parent->provider().variant();
  connect(
      object, &RequestNotifier::finishedItem, this, [=](EitherError<IItem> e) {
        set_done(true);
        emit createdDirectory();
        if (e.left())
          return emit context->errorOccurred("CreateDirectory", provider,
                                             e.left()->code_,
                                             e.left()->description_.c_str());
      });
  auto p = parent->provider().provider_;
  auto r = p->createDirectoryAsync(parent->item(), name.toStdString(),
                                   [object](EitherError<IItem> e) {
                                     emit object->finishedItem(e);
                                     object->deleteLater();
                                   });
  context->add(p, std::move(r));
}

void DeleteItemRequest::update(CloudContext* context, CloudItem* item) {
  set_done(false);
  auto object = new RequestNotifier;
  auto provider = item->provider().variant();
  connect(
      object, &RequestNotifier::finishedVoid, this, [=](EitherError<void> e) {
        set_done(true);
        emit itemDeleted();
        if (e.left())
          return emit context->errorOccurred("DeleteItem", provider,
                                             e.left()->code_,
                                             e.left()->description_.c_str());
      });
  auto p = item->provider().provider_;
  auto r = p->deleteItemAsync(item->item(), [object](EitherError<void> e) {
    emit object->finishedVoid(e);
    object->deleteLater();
  });
  context->add(p, std::move(r));
}

void RenameItemRequest::update(CloudContext* context, CloudItem* item,
                               QString name) {
  set_done(false);
  auto object = new RequestNotifier;
  auto provider = item->provider().variant();
  connect(
      object, &RequestNotifier::finishedItem, this, [=](EitherError<IItem> e) {
        set_done(true);
        emit itemRenamed();
        if (e.left())
          return context->errorOccurred("RenameItem", provider, e.left()->code_,
                                        e.left()->description_.c_str());
      });
  auto p = item->provider().provider_;
  auto r = p->renameItemAsync(item->item(), name.toStdString(),
                              [object](EitherError<IItem> e) {
                                emit object->finishedItem(e);
                                object->deleteLater();
                              });
  context->add(p, std::move(r));
}

void MoveItemRequest::update(CloudContext* context, CloudItem* source,
                             CloudItem* destination) {
  set_done(false);
  auto object = new RequestNotifier;
  auto provider = source->provider().variant();
  connect(
      object, &RequestNotifier::finishedItem, this, [=](EitherError<IItem> e) {
        set_done(true);
        emit itemMoved();
        if (e.left())
          return emit context->errorOccurred("MoveItem", provider,
                                             e.left()->code_,
                                             e.left()->description_.c_str());
      });
  auto p = source->provider().provider_;
  auto r = p->moveItemAsync(source->item(), destination->item(),
                            [object](EitherError<IItem> e) {
                              emit object->finishedItem(e);
                              object->deleteLater();
                            });
  context->add(p, std::move(r));
}

void UploadItemRequest::update(CloudContext* context, CloudItem* parent,
                               QString path, QString filename) {
  set_done(false);
  class UploadCallback : public IUploadFileCallback {
   public:
    UploadCallback(RequestNotifier* notifier, QString path)
        : notifier_(notifier), file_(path) {
      file_.open(QFile::ReadOnly);
    }

    void reset() override { file_.reset(); }

    uint32_t putData(char* data, uint32_t length) override {
      return static_cast<uint32_t>(
          file_.read(data, static_cast<qint64>(length)));
    }

    uint64_t size() override { return static_cast<uint64_t>(file_.size()); }

    void progress(uint64_t total, uint64_t now) override {
      emit notifier_->progressChanged(static_cast<qint64>(total),
                                      static_cast<qint64>(now));
    }

    void done(EitherError<IItem> e) override {
      emit notifier_->finishedItem(e);
      emit notifier_->progressChanged(0, 0);
      notifier_->deleteLater();
    }

   private:
    RequestNotifier* notifier_;
    File file_;
  };
  auto object = new RequestNotifier;
  auto provider = parent->provider().variant();
  connect(
      object, &RequestNotifier::finishedItem, this, [=](EitherError<IItem> e) {
        set_done(true);
        emit uploadComplete();
        if (e.left())
          return emit context->errorOccurred("UploadItem", provider,
                                             e.left()->code_,
                                             e.left()->description_.c_str());
      });
  connect(object, &RequestNotifier::progressChanged, this,
          [=](qint64 total, qint64 now) {
            qreal nprogress = static_cast<qreal>(now) / total;
            if (qAbs(progress_ - nprogress) > 0.01) {
              progress_ = nprogress;
              emit progressChanged();
            }
          });
  auto p = parent->provider().provider_;
  auto r = p->uploadFileAsync(parent->item(), filename.toStdString(),
                              util::make_unique<UploadCallback>(object, path));
  context->add(p, std::move(r));
}

void DownloadItemRequest::update(CloudContext* context, CloudItem* item,
                                 QString path) {
  set_done(false);
  auto object = new RequestNotifier;
  auto provider = item->provider().variant();
  connect(
      object, &RequestNotifier::finishedVoid, this, [=](EitherError<void> e) {
        set_done(true);
        emit downloadComplete();
        if (e.left())
          return emit context->errorOccurred("DownloadItem", provider,
                                             e.left()->code_,
                                             e.left()->description_.c_str());
      });
  connect(object, &RequestNotifier::progressChanged, this,
          [=](qint64 total, qint64 now) {
            qreal nprogress = static_cast<qreal>(now) / total;
            if (qAbs(progress_ - nprogress) > 0.01) {
              progress_ = nprogress;
              emit progressChanged();
            }
          });
  auto p = item->provider().provider_;
  auto r = p->downloadFileAsync(
      item->item(), util::make_unique<DownloadCallback>(object, path));
  context->add(p, std::move(r));
}

IHttpServer::IResponse::Pointer DispatchCallback::handle(
    const IHttpServer::IRequest& r) {
  auto state = first_url_part(r.url());
  auto cb = callback(state);
  if (!cb)
    return util::response_from_string(r, IHttpRequest::Bad, {},
                                      "invalid state");
  return cb->handle(r);
}

void DispatchCallback::add(const std::string& ssid,
                           IHttpServer::ICallback::Pointer cb) {
  std::lock_guard<std::mutex> lock(lock_);
  client_callback_[ssid] = cb;
}

void DispatchCallback::remove(const std::string& ssid) {
  std::lock_guard<std::mutex> lock(lock_);
  client_callback_.erase(ssid);
}

IHttpServer::ICallback::Pointer DispatchCallback::callback(
    const std::string& ssid) const {
  std::lock_guard<std::mutex> lock(lock_);
  auto it = client_callback_.find(ssid);
  if (it == client_callback_.end())
    return nullptr;
  else
    return it->second;
}

HttpServerWrapper::HttpServerWrapper(std::shared_ptr<DispatchCallback> d,
                                     IHttpServer::ICallback::Pointer callback,
                                     const std::string& session)
    : dispatch_(d), session_(session) {
  dispatch_->add(session, callback);
}

HttpServerWrapper::~HttpServerWrapper() { dispatch_->remove(session_); }

IHttpServer::ICallback::Pointer HttpServerWrapper::callback() const {
  return dispatch_->callback(session_);
}

ServerWrapperFactory::ServerWrapperFactory(
    std::shared_ptr<IHttpServerFactory> factory)
    : callback_(std::make_shared<DispatchCallback>()),
      http_server_(
          factory->create(callback_, "", IHttpServer::Type::Authorization)) {}

IHttpServer::Pointer ServerWrapperFactory::create(
    IHttpServer::ICallback::Pointer callback, const std::string& session_id,
    IHttpServer::Type) {
  return util::make_unique<HttpServerWrapper>(callback_, callback, session_id);
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
