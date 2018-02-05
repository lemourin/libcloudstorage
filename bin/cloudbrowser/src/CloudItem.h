#ifndef CLOUDITEM_H
#define CLOUDITEM_H

#include <QObject>
#include <QVariant>
#include <memory>
#include "ICloudProvider.h"

struct ListDirectoryCacheKey;

struct Provider {
  std::string label_;
  std::shared_ptr<cloudstorage::ICloudProvider> provider_;

  QVariantMap variant() const {
    QVariantMap map;
    map["label"] = label_.c_str();
    map["type"] = provider_->name().c_str();
    return map;
  }
};

class CloudItem : public QObject {
 public:
  Q_PROPERTY(QString filename READ filename CONSTANT)
  Q_PROPERTY(uint64_t size READ size CONSTANT)
  Q_PROPERTY(QString type READ type CONSTANT)

  CloudItem(const Provider&, cloudstorage::IItem::Pointer,
            QObject* parent = nullptr);

  cloudstorage::IItem::Pointer item() const { return item_; }
  const Provider& provider() const { return provider_; }
  QString filename() const;
  uint64_t size() const { return item_->size(); }
  QString type() const;
  Q_INVOKABLE bool supports(QString operation) const;
  ListDirectoryCacheKey key() const;

 private:
  Provider provider_;
  cloudstorage::IItem::Pointer item_;
  Q_OBJECT
};

Q_DECLARE_METATYPE(CloudItem*)

#endif  // CLOUDITEM_H
