#ifndef GETTHUMBNAILREQUEST_H
#define GETTHUMBNAILREQUEST_H

#include "Request/CloudRequest.h"

#include <QObject>
#include <QString>

class CloudContext;
class CloudItem;

class CLOUDBROWSER_API GetThumbnailRequest : public Request {
 public:
  Q_PROPERTY(QString source READ source NOTIFY sourceChanged)

  GetThumbnailRequest();
  ~GetThumbnailRequest();

  QString source() const { return source_; }
  Q_INVOKABLE void update(CloudContext* context, CloudItem* item);
  static QString thumbnail_path(const QString& filename);

 signals:
  void sourceChanged();
  void cacheFileAdded(quint64);

 private:
  QString source_;
  std::shared_ptr<std::atomic_bool> interrupt_;

  Q_OBJECT
};

#endif  // GETTHUMBNAILREQUEST_H
