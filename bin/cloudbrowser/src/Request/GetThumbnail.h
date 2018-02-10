#ifndef GETTHUMBNAILREQUEST_H
#define GETTHUMBNAILREQUEST_H

#include "Request/CloudRequest.h"

#include <QObject>
#include <QString>

class CloudContext;
class CloudItem;

class GetThumbnailRequest : public Request {
 public:
  Q_PROPERTY(QString source READ source NOTIFY sourceChanged)

  QString source() const { return source_; }
  Q_INVOKABLE void update(CloudContext* context, CloudItem* item);
  static QString thumbnail_path(const QString& filename);

 signals:
  void sourceChanged();
  void cacheFileAdded(quint64);

 private:
  QString source_;

  Q_OBJECT
};

#endif  // GETTHUMBNAILREQUEST_H
