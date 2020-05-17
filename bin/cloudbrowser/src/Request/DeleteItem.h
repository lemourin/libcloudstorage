#ifndef DELETEITEMREQUEST_H
#define DELETEITEMREQUEST_H

#include "Request/CloudRequest.h"

class CloudContext;
class CloudItem;

class DeleteItemRequest : public Request {
 public:
  Q_INVOKABLE void update(CloudContext*, CloudItem*);

 signals:
  void itemDeleted();

 private:
  Q_OBJECT
};

#endif  // DELETEITEMREQUEST_H
