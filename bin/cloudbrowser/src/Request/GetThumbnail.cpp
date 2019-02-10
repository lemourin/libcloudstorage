#include "GetThumbnail.h"

#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QStandardPaths>
#include <QThreadPool>
#include <QUrl>

#include "CloudContext.h"
#include "GenerateThumbnail.h"
#include "Utility/Utility.h"

using namespace cloudstorage;

namespace {

const auto MAX_THUMBNAIL_GENERATION_TIME = std::chrono::seconds(10);
const auto CHECK_INTERVAL = std::chrono::milliseconds(100);

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

class DownloadThumbnailCallback : public IDownloadFileCallback {
 public:
  DownloadThumbnailCallback(
      std::shared_ptr<IThreadPool> pool, RequestNotifier* notifier,
      std::shared_ptr<ICloudProvider> p, IItem::Pointer item,
      std::function<bool(std::chrono::system_clock::time_point)> interrupt)
      : pool_(pool),
        notifier_(notifier),
        provider_(p),
        item_(item),
        interrupt_(interrupt) {}

  void receivedData(const char* data, uint32_t length) override {
    data_ += std::string(data, length);
  }

  void progress(uint64_t total, uint64_t now) override {
    emit notifier_->progressChanged(static_cast<qint64>(total),
                                    static_cast<qint64>(now));
  }

  static void submit(RequestNotifier* notifier, QString path,
                     const std::string& data) {
    auto e = save(path, data);
    if (e.left()) {
      emit notifier->finishedVariant(e.left());
    } else {
      QVariantMap result;
      result["length"] = QVariant::fromValue(data.size());
      result["path"] = e.right()->c_str();
      emit notifier->finishedVariant(QVariant::fromValue(result));
    }
    notifier->deleteLater();
  }

  static EitherError<std::string> save(QString path, const std::string& data) {
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
      auto path =
          GetThumbnailRequest::thumbnail_path(item_->filename().c_str());
      auto provider = provider_;
      auto item = item_;
      auto notifier = notifier_;
      auto interrupt = interrupt_;
      pool_->schedule([path, provider, item, notifier, interrupt] {
        std::promise<EitherError<std::string>> promise;
        auto d = provider->getFileDaemonUrlAsync(
            item, [&](EitherError<std::string> e) { promise.set_value(e); });
        auto future = promise.get_future();
        std::future_status status = std::future_status::deferred;
        while (!interrupt(std::chrono::system_clock::now()) &&
               status != std::future_status::ready) {
          status = future.wait_for(CHECK_INTERVAL);
        }
        if (status != std::future_status::ready) d->cancel();
        auto r = future.get();
        if (r.left()) {
          emit notifier->finishedVariant(r.left());
          return notifier->deleteLater();
        }
        auto e = generate_thumbnail(*r.right(), interrupt);
        if (e.left()) {
          emit notifier->finishedVariant(e.left());
          return notifier->deleteLater();
        }
        submit(notifier, path, *e.right());
      });
      return;
    }
#endif
    if (e.left())
      emit notifier_->finishedVariant(e.left());
    else {
      auto notifier = notifier_;
      auto path =
          GetThumbnailRequest::thumbnail_path(item_->filename().c_str());
      auto data = std::move(data_);
      pool_->schedule(
          [notifier, path, data] { submit(notifier, path, std::move(data)); });
    }
  }

 private:
  std::shared_ptr<IThreadPool> pool_;
  RequestNotifier* notifier_;
  std::string data_;
  std::shared_ptr<ICloudProvider> provider_;
  IItem::Pointer item_;
  std::function<bool(std::chrono::system_clock::time_point)> interrupt_;
};

}  // namespace

GetThumbnailRequest::GetThumbnailRequest()
    : interrupt_(std::make_shared<std::atomic_bool>()) {}

GetThumbnailRequest::~GetThumbnailRequest() { *interrupt_ = true; }

void GetThumbnailRequest::update(CloudContext* context, CloudItem* item) {
  if (item->type() == "directory") return set_done(true);
  set_done(false);
  auto path = thumbnail_path(item->filename());
  QFile file(path);
  if (file.exists() && file.size() > 0) {
    source_ = QUrl::fromLocalFile(path).toString();
    emit sourceChanged();
    set_done(true);
  } else {
    auto object = new RequestNotifier;
    auto provider = item->provider().variant();
    connect(this, &Request::errorOccurred, context,
            [=](QVariantMap provider, int code, QString description) {
              emit context->errorOccurred("GetThumbnail", provider, code,
                                          description);
            });
    connect(this, &GetThumbnailRequest::cacheFileAdded, context,
            [=](quint64 size) { context->addCacheSize(size); });
    connect(object, &RequestNotifier::finishedVariant, this,
            [=](EitherError<QVariant> e) {
              if (e.left()) {
                set_done(true);
                emit errorOccurred(provider, e.left()->code_,
                                   e.left()->description_.c_str());
              } else {
                auto map = e.right()->toMap();
                source_ =
                    QUrl::fromLocalFile(map["path"].toString()).toString();
                emit sourceChanged();
                emit cacheFileAdded(map["length"].toULongLong());
                set_done(true);
              }
            });
    auto p = item->provider().provider_;
    auto interrupt = interrupt_;
    auto ctx_interrupt = context->interrupt();
    auto r = p->getThumbnailAsync(
        item->item(),
        util::make_unique<DownloadThumbnailCallback>(
            context->thumbnailer_thread_pool(), object, p, item->item(),
            [=](std::chrono::system_clock::time_point start_time) {
              return *interrupt || *ctx_interrupt ||
                     std::chrono::system_clock::now() - start_time >
                         MAX_THUMBNAIL_GENERATION_TIME;
              ;
            }));
    context->add(p, std::move(r));
  }
}

QString GetThumbnailRequest::thumbnail_path(const QString& filename) {
  return QStandardPaths::writableLocation(QStandardPaths::CacheLocation) +
         QDir::separator() + CloudContext::sanitize(filename) + "-thumbnail";
}

#ifdef WITH_THUMBNAILER
class Generator : public QQuickImageResponse {
 public:
  Generator(const std::string& url, int timestamp)
      : task_(new Runnable(this, url, timestamp)) {
    task_->setAutoDelete(false);
    QThreadPool::globalInstance()->start(task_);
  }

  ~Generator() override { delete task_; }

  void cancel() override { task_->cancelled_ = true; }

  QString errorString() const override { return error_; }

  QQuickTextureFactory* textureFactory() const override {
    return QQuickTextureFactory::textureFactoryForImage(image_);
  }

 private:
  class Runnable : public QRunnable {
   public:
    Runnable(Generator* r, const std::string& url, int timestamp)
        : response_(r), url_(url), timestamp_(timestamp), cancelled_(false) {}

    void run() override {
      try {
        auto data = generate_thumbnail(
            url_, timestamp_, [this](std::chrono::system_clock::time_point) {
              return bool(cancelled_);
            });
        if (data.left()) throw std::logic_error(data.left()->description_);
        response_->image_ = QImage::fromData(
            reinterpret_cast<const uchar*>(data.right()->c_str()),
            data.right()->size());
        emit response_->finished();
      } catch (const std::exception& e) {
        response_->error_ = e.what();
        emit response_->finished();
      }
    }

    std::atomic_bool cancelled_;

   private:
    Generator* response_;
    std::string url_;
    int timestamp_;
  };

  Runnable* task_;
  QString error_;
  QImage image_;
};

QQuickImageResponse* ThumbnailGenerator::requestImageResponse(const QString& id,
                                                              const QSize&) {
  auto timestamp = id.left(id.indexOf('/')).toLongLong() * 10000;
  auto url = id.right(id.length() - id.indexOf('/') - 1);
  return new Generator(url.toStdString(), timestamp);
}
#endif
