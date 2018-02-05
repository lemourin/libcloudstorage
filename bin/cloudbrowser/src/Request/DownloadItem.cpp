#include "DownloadItem.h"

#include "CloudContext.h"
#include "CloudItem.h"
#include "File.h"
#include "Utility/Utility.h"

#include <QUrl>

using namespace cloudstorage;

namespace {

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

void DownloadItemRequest::update(CloudContext* context, CloudItem* item,
                                 QString path) {
  set_done(false);
  auto object = new RequestNotifier;
  auto provider = item->provider().variant();
  connect(this, &Request::errorOccurred, context,
          [=](QVariantMap provider, int code, QString description) {
            emit context->errorOccurred("DownloadItem", provider, code,
                                        description);
          });
  connect(object, &RequestNotifier::finishedVoid, this,
          [=](EitherError<void> e) {
            set_done(true);
            emit downloadComplete();
            if (e.left())
              emit errorOccurred(provider, e.left()->code_,
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
