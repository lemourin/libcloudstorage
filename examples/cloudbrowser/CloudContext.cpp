#include "CloudContext.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlEngine>
#include <QSettings>
#include <QUrl>
#include "File.h"
#include "ICloudStorage.h"
#include "Utility/CurlHttp.h"
#include "Utility/MicroHttpdServer.h"
#include "Utility/Utility.h"

#ifdef WITH_THUMBNAILER
#include "GenerateThumbnail.h"
#endif

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
      http_server_factory_(util::make_unique<ServerWrapperFactory>(
          util::make_unique<MicroHttpdServerFactory>())),
      http_(util::make_unique<curl::CurlHttp>()) {
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
          [](QString operation, int code, QString description) {
            util::log(operation.toStdString() + ":", code,
                      description.toStdString());
          });
}

CloudContext::~CloudContext() {
  save();
  util::log_stream(util::make_unique<std::ostream>(std::cerr.rdbuf()));
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
}

QStringList CloudContext::providers() const {
  QStringList list;
  for (auto&& p : ICloudStorage::create()->providers()) list.append(p.c_str());
  return list;
}

QVariantList CloudContext::userProviders() const {
  std::lock_guard<std::mutex> lock(mutex_);
  QVariantList result;
  for (auto&& i : provider_) {
    QVariantMap v;
    v["type"] = i.provider_->name().c_str();
    v["label"] = i.label_.c_str();
    result.push_back(v);
  }
  return result;
}

QString CloudContext::authorizationUrl(QString provider) const {
  ICloudProvider::InitData data;
  data.permission_ = ICloudProvider::Permission::ReadWrite;
  data.hints_["state"] = provider.toStdString();
  return ICloudStorage::create()
      ->provider(provider.toStdString().c_str(), std::move(data))
      ->authorizeLibraryUrl()
      .c_str();
}

QObject* CloudContext::root(QVariant provider) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto label = provider.toMap()["label"].toString().toStdString();
  for (auto&& i : provider_)
    if (i.label_ == label) {
      auto item = new CloudItem(i.provider_, i.provider_->rootDirectory());
      QQmlEngine::setObjectOwnership(item, QQmlEngine::JavaScriptOwnership);
      return item;
    }
  return nullptr;
}

void CloudContext::removeProvider(QVariant provider) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto label = provider.toMap()["label"].toString().toStdString();
    std::vector<Provider> r;
    for (auto&& i : provider_)
      if (i.label_ != label) r.push_back(i);
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
      {"yandex", "Yandex Drive"},
      {"youtube", "YouTube"},
      {"gphotos", "Google Photos"}};
  auto it = name_map.find(provider.toStdString());
  if (it != name_map.end())
    return it->second.c_str();
  else
    return "";
}

void CloudContext::add(std::shared_ptr<ICloudProvider> p,
                       std::shared_ptr<IGenericRequest> r) {
  pool_.add(p, r);
}

void CloudContext::receivedCode(std::string provider, std::string code) {
  ICloudProvider::InitData data;
  data.permission_ = ICloudProvider::Permission::ReadWrite;
  auto p = ICloudStorage::create()->provider(provider, std::move(data));
  auto r = p->exchangeCodeAsync(code, [=](EitherError<Token> e) {
    if (e.left())
      return emit errorOccurred("ExchangeCode", e.left()->code_,
                                e.left()->description_.c_str());
    {
      std::lock_guard<std::mutex> lock(mutex_);
      int cnt = 0;
      for (auto&& i : provider_)
        if (i.provider_->name() == provider) cnt++;
      auto label = (pretty(provider.c_str()) + " #" + QString::number(cnt + 1))
                       .toStdString();
      provider_.push_back({label, this->provider(provider, label, *e.right())});
    }
    save();
    emit userProvidersChanged();
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
                                IHttpServer::Type type) {
      return factory_->create(cb, session_id, type);
    }

   private:
    std::shared_ptr<IHttpServerFactory> factory_;
  };
  class AuthCallback : public ICloudProvider::IAuthCallback {
   public:
    Status userConsentRequired(const ICloudProvider&) override {
      return Status::None;
    }

    void done(const ICloudProvider&, EitherError<void>) override {}
  };
  ICloudProvider::InitData data;
  data.permission_ = ICloudProvider::Permission::ReadWrite;
  data.token_ = token.token_;
  data.hints_["temporary_directory"] =
      QDir::toNativeSeparators(QDir::tempPath() + "/").toStdString();
  data.hints_["access_token"] = token.access_token_;
  data.hints_["state"] = util::to_base64(label);
  data.hints_["file_url"] = "http://localhost:12345";
  data.http_engine_ = util::make_unique<HttpWrapper>(http_);
  data.http_server_ =
      util::make_unique<HttpServerFactoryWrapper>(http_server_factory_);
  data.callback_ = util::make_unique<AuthCallback>();
  return ICloudStorage::create()->provider(name, std::move(data));
}

ICloudProvider* CloudContext::provider(const std::string& label) const {
  std::lock_guard<std::mutex> lock(mutex_);
  for (auto&& i : provider_)
    if (i.label_ == label) return i.provider_.get();
  return nullptr;
}

CloudContext::HttpServerCallback::HttpServerCallback(CloudContext* ctx)
    : ctx_(ctx) {}

IHttpServer::IResponse::Pointer CloudContext::HttpServerCallback::handle(
    const IHttpServer::IRequest& request) {
  auto state = request.get("state");
  auto code = request.get("code");
  if (code && state) {
    QFile file(":/resources/default_success.html");
    file.open(QFile::ReadOnly);
    ctx_->receivedCode(state, code);
    return util::response_from_string(request, IHttpRequest::Ok, {},
                                      file.readAll().constData());
  } else if (request.url() == "/login" && state) {
    QFile file(QString(":/resources/") + state + "_login.html");
    file.open(QFile::ReadOnly);
    return util::response_from_string(request, IHttpRequest::Ok, {},
                                      file.readAll().constData());
  } else
    return util::response_from_string(request, IHttpRequest::Bad, {},
                                      "Bad request");
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

CloudItem::CloudItem(std::shared_ptr<ICloudProvider> p, IItem::Pointer item,
                     QObject* parent)
    : QObject(parent), provider_(p), item_(item) {}

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

ListDirectoryRequest::ListDirectoryRequest(QObject* parent) : Request(parent) {
  connect(this, &ListDirectoryRequest::itemChanged, this,
          &ListDirectoryRequest::update);
}

void ListDirectoryRequest::set_item(CloudItem* item) {
  if (item_ == item) return;
  item_ = item;
  emit contextChanged();
}

QQmlListProperty<CloudItem> ListDirectoryRequest::list() {
  QQmlListProperty<CloudItem> r(
      this, this,
      [](QQmlListProperty<CloudItem>* lst) -> int {
        auto t = reinterpret_cast<ListDirectoryRequest*>(lst->data);
        return static_cast<int>(t->list_.size());
      },
      [](QQmlListProperty<CloudItem>* lst, int idx) {
        auto t = reinterpret_cast<ListDirectoryRequest*>(lst->data);
        auto item = new CloudItem(t->item_->provider(),
                                  t->list_[static_cast<size_t>(idx)]);
        QQmlEngine::setObjectOwnership(item, QQmlEngine::JavaScriptOwnership);
        return item;
      });
  return r;
}

void ListDirectoryRequest::update() {
  if (!item() || !context()) return;
  set_done(false);
  auto object = new RequestNotifier;
  connect(object, &RequestNotifier::finishedList, this,
          [=](EitherError<std::vector<IItem::Pointer>> r) {
            set_done(true);
            if (r.left())
              return emit context()->errorOccurred(
                  "ListDirectory", r.left()->code_,
                  r.left()->description_.c_str());
            list_ = *r.right();
            emit listChanged();
          });
  auto r = item_->provider()->listDirectoryAsync(
      item_->item(), [object](EitherError<std::vector<IItem::Pointer>> r) {
        emit object->finishedList(r);
        object->deleteLater();
      });
  context()->add(item_->provider(), std::move(r));
}

Request::Request(QObject* parent) : QObject(parent), context_(), done_() {}

void Request::set_context(CloudContext* ctx) {
  if (context_ == ctx) return;
  context_ = ctx;
  emit contextChanged();
}

void Request::set_done(bool done) {
  if (done_ == done) return;
  done_ = done;
  emit doneChanged();
}

void Request::classBegin() {
  connect(this, &Request::contextChanged, this, &Request::update);
}

void Request::componentComplete() {}

GetThumbnailRequest::GetThumbnailRequest(QObject* parent) : Request(parent) {
  connect(this, &GetThumbnailRequest::itemChanged, this,
          &GetThumbnailRequest::update);
}

void GetThumbnailRequest::set_item(CloudItem* item) {
  if (item_ == item) return;
  item_ = item;
  emit contextChanged();
}

void GetThumbnailRequest::update() {
  if (!item() || !context()) return;
  set_done(false);
  auto path = QDir::tempPath() + QDir::separator() +
              sanitize(item_->filename()) + "-thumbnail";
  QFile file(path);
  if (file.exists() && file.size() > 0) {
    source_ = QUrl::fromLocalFile(path).toString();
    set_done(true);
    emit sourceChanged();
  } else {
    auto object = new RequestNotifier;
    connect(object, &RequestNotifier::finishedVoid, this,
            [=](EitherError<void> e) {
              set_done(true);
              if (e.left())
                return emit context()->errorOccurred(
                    "GetThumbnail", e.left()->code_,
                    e.left()->description_.c_str());
              source_ = QUrl::fromLocalFile(path).toString();
              emit sourceChanged();
            });
    class DownloadThumbnailCallback : public IDownloadFileCallback {
     public:
      DownloadThumbnailCallback(RequestNotifier* notifier, QUrl path,
                                QString filename,
                                std::shared_ptr<ICloudProvider> p,
                                IItem::Pointer item)
          : notifier_(notifier),
            path_(path.toLocalFile() + QDir::separator() + filename),
            file_(path_),
            provider_(p),
            item_(item) {
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
#ifdef WITH_THUMBNAILER
        if (e.left() && (item_->type() == IItem::FileType::Image ||
                         item_->type() == IItem::FileType::Video)) {
          auto provider = provider_;
          auto item = item_;
          auto path = path_;
          auto notifier = notifier_;
          std::thread([provider, item, notifier, path] {
            auto r = provider->getItemUrlAsync(item)->result();
            if (r.left()) {
              emit notifier->finishedVoid(r.left());
              return notifier->deleteLater();
            }
            auto e = generate_thumbnail(*r.right());
            if (e.left()) {
              emit notifier->finishedVoid(e.left());
              return notifier->deleteLater();
            }
            {
              QFile file(path);
              file.open(QFile::WriteOnly);
              file.write(e.right()->data(),
                         static_cast<qint64>(e.right()->size()));
            }
            emit notifier->finishedVoid(nullptr);
            notifier->deleteLater();
          }).detach();
          return;
        }
#endif
        emit notifier_->finishedVoid(e);
        notifier_->deleteLater();
      }

     private:
      RequestNotifier* notifier_;
      QString path_;
      QFile file_;
      std::shared_ptr<ICloudProvider> provider_;
      IItem::Pointer item_;
    };
    auto p = item()->provider();
    auto r = p->getThumbnailAsync(
        item()->item(),
        util::make_unique<DownloadThumbnailCallback>(
            object,
            QUrl::fromLocalFile(QDir::tempPath() + QDir::separator())
                .toString(),
            sanitize(item_->filename()) + "-thumbnail", p, item()->item()));
    context()->add(p, std::move(r));
  }
}

GetUrlRequest::GetUrlRequest(QObject* parent) : Request(parent) {
  connect(this, &GetUrlRequest::itemChanged, this, &GetUrlRequest::update);
}

void GetUrlRequest::set_item(CloudItem* item) {
  if (item_ == item) return;
  item_ = item;
  emit itemChanged();
}

void GetUrlRequest::update() {
  if (!item() || !context()) return;
  set_done(false);
  auto object = new RequestNotifier;
  connect(object, &RequestNotifier::finishedString, this,
          [=](EitherError<std::string> e) {
            set_done(true);
            if (e.left())
              return emit context()->errorOccurred(
                  "GetItemUrl", e.left()->code_,
                  e.left()->description_.c_str());
            source_ = e.right()->c_str();
            emit sourceChanged();
          });
  auto p = item_->provider();
  auto r =
      p->getItemUrlAsync(item_->item(), [object](EitherError<std::string> e) {
        emit object->finishedString(e);
        object->deleteLater();
      });
  context()->add(p, std::move(r));
}

CreateDirectoryRequest::CreateDirectoryRequest(QObject* parent)
    : Request(parent) {
  connect(this, &CreateDirectoryRequest::parentChanged, this,
          &CreateDirectoryRequest::update);
  connect(this, &CreateDirectoryRequest::nameChanged, this,
          &CreateDirectoryRequest::update);
}

void CreateDirectoryRequest::set_parent(CloudItem* parent) {
  if (parent_ == parent) return;
  parent_ = parent;
  emit parentChanged();
}

void CreateDirectoryRequest::set_name(QString name) {
  if (name_ == name) return;
  name_ = name;
  emit nameChanged();
}

void CreateDirectoryRequest::update() {
  if (!context() || name_.isEmpty() || !parent_) return;
  set_done(false);
  auto object = new RequestNotifier;
  connect(
      object, &RequestNotifier::finishedItem, this, [=](EitherError<IItem> e) {
        set_done(true);
        emit createdDirectory();
        if (e.left())
          return emit context()->errorOccurred("CreateDirectory",
                                               e.left()->code_,
                                               e.left()->description_.c_str());
      });
  auto p = parent_->provider();
  auto r = p->createDirectoryAsync(parent_->item(), name_.toStdString(),
                                   [object](EitherError<IItem> e) {
                                     emit object->finishedItem(e);
                                     object->deleteLater();
                                   });
  context()->add(p, std::move(r));
}

DeleteItemRequest::DeleteItemRequest(QObject* parent) : Request(parent) {
  connect(this, &DeleteItemRequest::itemChanged, this,
          &DeleteItemRequest::update);
}

void DeleteItemRequest::set_item(CloudItem* item) {
  if (item_ == item) return;
  item_ = item;
  emit itemChanged();
}

void DeleteItemRequest::update() {
  if (!context() || !item()) return;
  set_done(false);
  auto object = new RequestNotifier;
  connect(
      object, &RequestNotifier::finishedVoid, this, [=](EitherError<void> e) {
        set_done(true);
        emit itemDeleted();
        if (e.left())
          return emit context()->errorOccurred("DeleteItem", e.left()->code_,
                                               e.left()->description_.c_str());
      });
  auto p = item_->provider();
  auto r = p->deleteItemAsync(item_->item(), [object](EitherError<void> e) {
    emit object->finishedVoid(e);
    object->deleteLater();
  });
  context()->add(p, std::move(r));
}

RenameItemRequest::RenameItemRequest(QObject* parent) : Request(parent) {
  connect(this, &RenameItemRequest::itemChanged, this,
          &RenameItemRequest::update);
  connect(this, &RenameItemRequest::nameChanged, this,
          &RenameItemRequest::update);
}

void RenameItemRequest::set_item(CloudItem* item) {
  if (item_ == item) return;
  item_ = item;
  emit itemChanged();
}

void RenameItemRequest::set_name(QString name) {
  if (name_ == name) return;
  name_ = name;
  emit nameChanged();
}

void RenameItemRequest::update() {
  if (!context() || !item() || name().isEmpty()) return;
  set_done(false);
  auto object = new RequestNotifier;
  connect(object, &RequestNotifier::finishedItem, this,
          [=](EitherError<IItem> e) {
            set_done(true);
            name_ = "";
            emit itemRenamed();
            if (e.left())
              return context()->errorOccurred("RenameItem", e.left()->code_,
                                              e.left()->description_.c_str());
          });
  auto p = item_->provider();
  auto r = p->renameItemAsync(item_->item(), name().toStdString(),
                              [object](EitherError<IItem> e) {
                                emit object->finishedItem(e);
                                object->deleteLater();
                              });
  context()->add(p, std::move(r));
}

MoveItemRequest::MoveItemRequest(QObject* parent) : Request(parent) {
  connect(this, &MoveItemRequest::sourceChanged, this,
          &MoveItemRequest::update);
  connect(this, &MoveItemRequest::destinationChanged, this,
          &MoveItemRequest::update);
}

void MoveItemRequest::set_source(CloudItem* source) {
  if (source_ == source) return;
  source_ = source;
  emit sourceChanged();
}

void MoveItemRequest::set_destination(CloudItem* destination) {
  if (destination_ == destination) return;
  destination_ = destination;
  emit destinationChanged();
}

void MoveItemRequest::update() {
  if (!context() || !source() || !destination()) return;
  set_done(false);
  auto object = new RequestNotifier;
  connect(object, &RequestNotifier::finishedItem, this,
          [=](EitherError<IItem> e) {
            set_done(true);
            set_source(nullptr);
            set_destination(nullptr);
            emit itemMoved();
            if (e.left())
              return emit context()->errorOccurred(
                  "MoveItem", e.left()->code_, e.left()->description_.c_str());
          });
  auto p = source_->provider();
  auto r = p->moveItemAsync(source_->item(), destination_->item(),
                            [object](EitherError<IItem> e) {
                              emit object->finishedItem(e);
                              object->deleteLater();
                            });
  context()->add(p, std::move(r));
}

UploadItemRequest::UploadItemRequest(QObject* parent) : Request(parent) {
  connect(this, &UploadItemRequest::parentChanged, this,
          &UploadItemRequest::update);
  connect(this, &UploadItemRequest::pathChanged, this,
          &UploadItemRequest::update);
}

void UploadItemRequest::set_parent(CloudItem* parent) {
  if (parent_ == parent) return;
  parent_ = parent;
  emit parentChanged();
}

void UploadItemRequest::set_path(QString path) {
  if (path_ == path) return;
  path_ = path;
  emit pathChanged();
}

void UploadItemRequest::set_filename(QString filename) {
  if (filename_ == filename) return;
  filename_ = filename;
  emit filenameChanged();
}

void UploadItemRequest::update() {
  if (!context() || !parent() || path().isEmpty() || filename().isEmpty())
    return;
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
  connect(
      object, &RequestNotifier::finishedItem, this, [=](EitherError<IItem> e) {
        set_done(true);
        emit uploadComplete();
        if (e.left())
          return emit context()->errorOccurred("UploadItem", e.left()->code_,
                                               e.left()->description_.c_str());
      });
  connect(object, &RequestNotifier::progressChanged, this,
          [=](qint64 total, qint64 now) {
            total_ = total;
            now_ = now;
            emit progressChanged();
          });
  auto p = parent()->provider();
  auto r =
      p->uploadFileAsync(parent()->item(), filename().toStdString(),
                         util::make_unique<UploadCallback>(object, path()));
  context()->add(p, std::move(r));
}

DownloadItemRequest::DownloadItemRequest(QObject* parent) : Request(parent) {
  connect(this, &DownloadItemRequest::itemChanged, this,
          &DownloadItemRequest::update);
  connect(this, &DownloadItemRequest::pathChanged, this,
          &DownloadItemRequest::update);
}

void DownloadItemRequest::set_item(CloudItem* item) {
  if (item_ == item) return;
  item_ = item;
  emit itemChanged();
}

void DownloadItemRequest::set_path(QString path) {
  if (path_ == path) return;
  path_ = path;
  emit pathChanged();
}

void DownloadItemRequest::set_filename(QString filename) {
  if (filename_ == filename) return;
  filename_ = filename;
  emit filenameChanged();
}

void DownloadItemRequest::update() {
  if (!context() || !item() || path().isEmpty()) return;
  set_done(false);
  auto object = new RequestNotifier;
  connect(
      object, &RequestNotifier::finishedVoid, this, [=](EitherError<void> e) {
        set_done(true);
        emit downloadComplete();
        if (e.left())
          return emit context()->errorOccurred("DownloadItem", e.left()->code_,
                                               e.left()->description_.c_str());
      });
  connect(object, &RequestNotifier::progressChanged, this,
          [=](qint64 total, qint64 now) {
            total_ = total;
            now_ = now;
            emit progressChanged();
          });
  auto p = item()->provider();
  auto r = p->downloadFileAsync(
      item()->item(),
      util::make_unique<DownloadCallback>(object, this->path()));
  context()->add(p, std::move(r));
}

IHttpServer::IResponse::Pointer DispatchCallback::handle(
    const IHttpServer::IRequest& r) {
  auto state = r.get("state");
  if (!state)
    return util::response_from_string(r, IHttpRequest::Bad, {}, "bad request");
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
