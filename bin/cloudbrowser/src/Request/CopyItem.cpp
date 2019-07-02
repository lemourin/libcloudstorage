#include "CopyItem.h"

#include <QTemporaryFile>

#include "Utility/Utility.h"

namespace {

class Upload : public cloudstorage::IUploadFileCallback {
 public:
  Upload(RequestNotifier* notifier, std::unique_ptr<QTemporaryFile> file)
      : notifier_(notifier), temp_file_(std::move(file)) {
    temp_file_->reset();
  }

  void done(cloudstorage::EitherError<cloudstorage::IItem> e) override {
    if (e.left()) {
      notifier_->finishedVoid(e.left());
    } else {
      notifier_->finishedVoid(nullptr);
    }
    notifier_->deleteLater();
  }

  uint32_t putData(char* data, uint32_t maxlength, uint64_t offset) override {
    temp_file_->seek(offset);
    return temp_file_->read(data, maxlength);
  }

  uint64_t size() override { return temp_file_->size(); }

  void progress(uint64_t total, uint64_t now) override {
    emit notifier_->progressChanged(2 * total, total + now);
  }

 private:
  RequestNotifier* notifier_;
  std::unique_ptr<QTemporaryFile> temp_file_;
};

class Download : public cloudstorage::IDownloadFileCallback {
 public:
  Download(RequestNotifier* notifier,
           std::shared_ptr<CloudContext::RequestPool> pool,
           cloudstorage::IItem::Pointer dest,
           std::shared_ptr<cloudstorage::ICloudProvider> p,
           std::string filename)
      : notifier_(notifier),
        temp_file_(cloudstorage::util::make_unique<QTemporaryFile>()),
        pool_(std::move(pool)),
        destination_(std::move(dest)),
        destination_provider_(std::move(p)),
        filename_(std::move(filename)) {
    temp_file_->open();
  }

  void done(cloudstorage::EitherError<void> e) override {
    if (e.left()) {
      notifier_->finishedVoid(e);
      notifier_->deleteLater();
    } else {
      auto r = destination_provider_->uploadFileAsync(
          destination_, filename_,
          std::make_shared<Upload>(notifier_, std::move(temp_file_)));
      pool_->add(destination_provider_, std::move(r));
    }
  }

  void receivedData(const char* data, uint32_t length) override {
    temp_file_->write(data, length);
  }

  void progress(uint64_t total, uint64_t now) override {
    emit notifier_->progressChanged(2 * total, now);
  }

 private:
  RequestNotifier* notifier_;
  std::unique_ptr<QTemporaryFile> temp_file_;
  std::shared_ptr<CloudContext::RequestPool> pool_;
  cloudstorage::IItem::Pointer destination_;
  std::shared_ptr<cloudstorage::ICloudProvider> destination_provider_;
  std::string filename_;
};

}  // namespace

void CopyItemRequest::update(CloudContext* context, CloudItem* source,
                             CloudItem* destination) {
  set_done(false);
  if (source->type() == "directory") {
    emit context->errorOccurred("CopyItem", source->provider().variant(),
                                cloudstorage::IHttpRequest::Failure,
                                "Can't copy a directory");
    return set_done(true);
  }
  if (destination->type() != "directory") {
    emit context->errorOccurred("CopyItem", source->provider().variant(),
                                cloudstorage::IHttpRequest::Failure,
                                "Destination is not a directory");
    return set_done(true);
  }
  auto object = new RequestNotifier;
  auto provider = source->provider().variant();
  connect(this, &Request::errorOccurred, context,
          [=](QVariantMap provider, int code, QString description) {
            emit context->errorOccurred("CopyItem", provider, code,
                                        description);
          });
  connect(object, &RequestNotifier::finishedVoid, this,
          [=](cloudstorage::EitherError<void> e) {
            set_done(true);
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

  auto p = source->provider().provider_;
  auto r = p->downloadFileAsync(
      source->item(),
      std::make_shared<Download>(
          object, context->request_pool(), destination->item(),
          destination->provider().provider_,
          CloudContext::sanitize(source->filename()).toStdString()));
  context->add(source->provider().provider_, std::move(r));
}
