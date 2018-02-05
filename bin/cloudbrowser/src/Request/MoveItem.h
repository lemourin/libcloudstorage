#ifndef MOVEITEMREQUEST_H
#define MOVEITEMREQUEST_H

#include "Request/CloudRequest.h"

class CloudContext;
class CloudItem;

class MoveItemRequest : public Request {
 public:
  Q_INVOKABLE void update(CloudContext*, CloudItem* source,
                          CloudItem* destination);

 signals:
  void itemMoved();

 private:
  Q_OBJECT
};

#endif  // MOVEITEMREQUEST_H
