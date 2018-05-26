#include "UploadItem.h"

#include "CloudContext.h"
#include "CloudItem.h"

#include <QFile>
#include "File.h"
#include "Utility/Utility.h"

using namespace cloudstorage;

namespace {
class UploadCallback : public IUploadFileCallback {
 public:
  UploadCallback(RequestNotifier* notifier, QString path)
      : notifier_(notifier), file_(path) {
    file_.open(QFile::ReadOnly);
  }

  uint32_t putData(char* data, uint32_t length, uint64_t offset) override {
    file_.seek(offset);
    return static_cast<uint32_t>(file_.read(data, static_cast<qint64>(length)));
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
}  // namespace

void UploadItemRequest::update(CloudContext* context, CloudItem* parent,
                               QString path, QString filename) {
  set_done(false);
  auto object = new RequestNotifier;
  auto provider = parent->provider().variant();
  connect(this, &Request::errorOccurred, context,
          [=](QVariantMap provider, int code, QString description) {
            emit context->errorOccurred("UploadItem", provider, code,
                                        description);
          });
  connect(object, &RequestNotifier::finishedItem, this,
          [=](EitherError<IItem> e) {
            set_done(true);
            emit uploadComplete();
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
  auto p = parent->provider().provider_;
  auto r = p->uploadFileAsync(parent->item(), filename.toStdString(),
                              util::make_unique<UploadCallback>(object, path));
  context->add(p, std::move(r));
}
