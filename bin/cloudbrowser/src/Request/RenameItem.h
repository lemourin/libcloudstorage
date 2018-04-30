#ifndef RENAMEITEMREQUEST_H
#define RENAMEITEMREQUEST_H

#include "Request/CloudRequest.h"

class CloudContext;
class CloudItem;

class CLOUDBROWSER_API RenameItemRequest : public Request {
 public:
  Q_INVOKABLE void update(CloudContext*, CloudItem* item, QString name);

 signals:
  void itemRenamed();

 private:
  Q_OBJECT
};

#endif  // RENAMEITEMREQUEST_H
