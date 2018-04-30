#ifndef CREATEDIRECTORYREQUEST_H
#define CREATEDIRECTORYREQUEST_H

#include "Request/CloudRequest.h"

#include <QObject>
#include <QString>

class CloudContext;
class CloudItem;

class CLOUDBROWSER_API CreateDirectoryRequest : public Request {
 public:
  Q_INVOKABLE void update(CloudContext*, CloudItem* parent, QString name);

 signals:
  void createdDirectory();

 private:
  Q_OBJECT
};

#endif  // CREATEDIRECTORYREQUEST_H
