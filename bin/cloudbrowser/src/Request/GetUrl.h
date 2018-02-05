#ifndef GETURLREQUEST_H
#define GETURLREQUEST_H

#include "Request/CloudRequest.h"

#include <QString>

class CloudContext;
class CloudItem;

class GetUrlRequest : public Request {
 public:
  Q_PROPERTY(QString source READ source NOTIFY sourceChanged)

  QString source() const { return source_; }
  Q_INVOKABLE void update(CloudContext*, CloudItem*);

 signals:
  void sourceChanged();

 private:
  QString source_;

  Q_OBJECT
};

#endif  // GETURLREQUEST_H
