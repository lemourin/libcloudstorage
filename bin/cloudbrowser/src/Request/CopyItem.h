#ifndef COPYREQUEST_H
#define COPYREQUEST_H

#include "CloudContext.h"
#include "CloudItem.h"
#include "CloudRequest.h"

class CLOUDBROWSER_API CopyItemRequest : public Request {
 public:
  Q_PROPERTY(qreal progress READ progress NOTIFY progressChanged)

  qreal progress() const { return progress_; }

  void update(CloudContext* context, CloudItem* source, CloudItem* destination);

 signals:
  void progressChanged();

 private:
  qreal progress_ = 0;

  Q_OBJECT
};

#endif  // COPYREQUEST_H
