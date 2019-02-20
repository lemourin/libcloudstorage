#ifndef GETTHUMBNAILREQUEST_H
#define GETTHUMBNAILREQUEST_H

#include "Request/CloudRequest.h"
#include "Utility/Utility.h"

#include <QObject>
#include <QQuickAsyncImageProvider>
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

#ifdef WITH_THUMBNAILER
class ThumbnailGenerator : public QQuickAsyncImageProvider {
 public:
  QQuickImageResponse* requestImageResponse(
      const QString& id, const QSize& requested_size) override;

 private:
  std::shared_ptr<
      cloudstorage::util::LRUCache<std::string, cloudstorage::IItem>>
      item_cache_ = std::make_shared<
          cloudstorage::util::LRUCache<std::string, cloudstorage::IItem>>(32);
};
#endif

#endif  // GETTHUMBNAILREQUEST_H
