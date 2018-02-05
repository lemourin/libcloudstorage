#ifndef DOWNLOADITEMREQUEST_H
#define DOWNLOADITEMREQUEST_H

#include "Request/CloudRequest.h"

class CloudContext;
class CloudItem;

class DownloadItemRequest : public Request {
 public:
  Q_PROPERTY(qreal progress READ progress NOTIFY progressChanged)

  qreal progress() const { return progress_; }

  Q_INVOKABLE void update(CloudContext*, CloudItem* item, QString path);

 signals:
  void progressChanged();
  void downloadComplete();

 private:
  qreal progress_ = 0;

  Q_OBJECT
};

#endif  // DOWNLOADITEMREQUEST_H
