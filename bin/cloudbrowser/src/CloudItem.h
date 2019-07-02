#ifndef CLOUDITEM_H
#define CLOUDITEM_H

#include <QObject>
#include <QVariant>
#include <memory>

#include "Exec.h"
#include "ICloudProvider.h"

struct ListDirectoryCacheKey;

struct CLOUDBROWSER_API Provider {
  std::string label_;
  std::shared_ptr<cloudstorage::ICloudProvider> provider_;

  QVariantMap variant() const {
    QVariantMap map;
    map["label"] = label_.c_str();
    map["type"] = provider_->name().c_str();
    return map;
  }
};

class CLOUDBROWSER_API CloudItem : public QObject {
 public:
  Q_PROPERTY(QString filename READ filename CONSTANT)
  Q_PROPERTY(qint64 size READ size CONSTANT)
  Q_PROPERTY(QString timestamp READ timestamp CONSTANT)
  Q_PROPERTY(QString type READ type CONSTANT)

  CloudItem(Provider, cloudstorage::IItem::Pointer, QObject* parent = nullptr);

  cloudstorage::IItem::Pointer item() const { return item_; }
  const Provider& provider() const { return provider_; }
  QString filename() const;
  qint64 size() const;
  QString timestamp() const;
  QString type() const;
  Q_INVOKABLE bool supports(const QString& operation) const;
  ListDirectoryCacheKey key() const;

 private:
  Provider provider_;
  cloudstorage::IItem::Pointer item_;
  Q_OBJECT
};

Q_DECLARE_METATYPE(CloudItem*)

#endif  // CLOUDITEM_H
