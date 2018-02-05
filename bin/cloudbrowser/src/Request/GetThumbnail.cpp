#include "GetThumbnail.h"

#include <QFile>
#include <QSaveFile>
#include <QUrl>
#include "CloudContext.h"
#include "GenerateThumbnail.h"
#include "Utility/Utility.h"

using namespace cloudstorage;

namespace {

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
  DownloadThumbnailCallback(CloudContext* context, RequestNotifier* notifier,
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
      auto path = CloudContext::thumbnail_path(item_->filename().c_str());
      auto provider = provider_;
      auto item = item_;
      auto notifier = notifier_;
      std::thread([path, provider, item, notifier] {
        auto r = provider->getItemUrlAsync(item)->result();
        if (r.left()) {
          emit notifier->finishedVariant(r.left());
          return notifier->deleteLater();
        }
        if (item->type() == IItem::FileType::Image) {
          if (item->size() < 2 * 1024 * 1024) {
            auto downloader = std::make_shared<DownloadToString>();
            auto e = provider->downloadFileAsync(item, downloader)->result();
            if (e.left())
              emit notifier->finishedVariant(e.left());
            else
              return submit(notifier, path, downloader->data());
          } else {
            emit notifier->finishedVariant(
                Error{IHttpRequest::ServiceUnavailable, "image too big"});
          }
          return notifier->deleteLater();
        }
        auto e = generate_thumbnail(*r.right());
        if (e.left()) {
          emit notifier->finishedVariant(e.left());
          return notifier->deleteLater();
        }
        submit(notifier, path, *e.right());
      }).detach();
      return;
    }
#endif
    if (e.left())
      emit notifier_->finishedVariant(e.left());
    else {
      auto notifier = notifier_;
      auto path = CloudContext::thumbnail_path(item_->filename().c_str());
      auto data = std::move(data_);
      context_->schedule(
          [notifier, path, data] { submit(notifier, path, std::move(data)); });
    }
  }

 private:
  CloudContext* context_;
  RequestNotifier* notifier_;
  std::string data_;
  std::shared_ptr<ICloudProvider> provider_;
  IItem::Pointer item_;
};

}  // namespace

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
    connect(this, &Request::errorOccurred, context,
            [=](QVariantMap provider, int code, QString description) {
              emit context->errorOccurred("GetThumbnail", provider, code,
                                          description);
            });
    connect(this, &GetThumbnailRequest::cacheFileAdded, context,
            [=](quint64 size) { context->addCacheSize(size); });
    connect(object, &RequestNotifier::finishedVariant, this,
            [=](EitherError<QVariant> e) {
              set_done(true);
              if (e.left())
                return errorOccurred(provider, e.left()->code_,
                                     e.left()->description_.c_str());
              auto map = e.right()->toMap();
              source_ = QUrl::fromLocalFile(map["path"].toString()).toString();
              emit sourceChanged();
              emit cacheFileAdded(map["length"].toULongLong());
            });
    auto p = item->provider().provider_;
    auto r = p->getThumbnailAsync(item->item(),
                                  util::make_unique<DownloadThumbnailCallback>(
                                      context, object, p, item->item()));
    context->add(p, std::move(r));
  }
}
