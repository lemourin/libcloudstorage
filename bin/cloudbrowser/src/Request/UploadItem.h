#ifndef UPLOADITEMREQUEST_H
#define UPLOADITEMREQUEST_H

#include "Request/CloudRequest.h"

class CloudContext;
class CloudItem;

class CLOUDBROWSER_API UploadItemRequest : public Request {
 public:
  Q_PROPERTY(qreal progress READ progress NOTIFY progressChanged)

  qreal progress() const { return progress_; }

  Q_INVOKABLE void update(CloudContext* context, CloudItem* parent,
                          QString path, QString filename);

 signals:
  void progressChanged();
  void uploadComplete();

 private:
  qreal progress_ = 0;

  Q_OBJECT
};

#endif  // UPLOADITEMREQUEST_H
