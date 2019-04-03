#ifndef LISTDIRECTORYREQUEST_H
#define LISTDIRECTORYREQUEST_H

#include <QAbstractListModel>
#include <string>
#include <unordered_set>
#include <utility>
#include "CloudItem.h"
#include "ICloudProvider.h"
#include "Request/CloudRequest.h"

class CloudContext;

struct CLOUDBROWSER_API ListDirectoryCacheKey {
  std::string provider_type_;
  std::string provider_label_;
  std::string directory_id_;

  bool operator==(const ListDirectoryCacheKey& d) const {
    return std::tie(provider_type_, provider_label_, directory_id_) ==
           std::tie(d.provider_type_, d.provider_label_, d.directory_id_);
  }
};

namespace std {
template <>
struct hash<ListDirectoryCacheKey> {
  size_t operator()(const ListDirectoryCacheKey& d) const {
    return std::hash<std::string>()(d.provider_type_ + "#" + d.provider_label_ +
                                    "$" + d.directory_id_);
  }
};
}  // namespace std

class CLOUDBROWSER_API ListDirectoryModel : public QAbstractListModel {
 public:
  void set_provider(const Provider&);
  void add(cloudstorage::IItem::Pointer);
  void clear();
  int find(cloudstorage::IItem::Pointer) const;
  void insert(int idx, cloudstorage::IItem::Pointer);
  void remove(int idx);
  const std::vector<cloudstorage::IItem::Pointer>& list() const {
    return list_;
  }
  void assign(const std::vector<cloudstorage::IItem::Pointer>&);

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int) const override;
  QHash<int, QByteArray> roleNames() const override;

 private:
  Provider provider_;
  std::vector<cloudstorage::IItem::Pointer> list_;
  std::unordered_multiset<std::string> id_;
  Q_OBJECT
};

class CLOUDBROWSER_API ListDirectoryRequest : public Request {
 public:
  Q_PROPERTY(ListDirectoryModel* list READ list CONSTANT)

  ListDirectoryModel* list();

  Q_INVOKABLE void update(CloudContext*, CloudItem*);

 signals:
  void itemChanged();
  void finished(ListDirectoryCacheKey,
                const std::vector<cloudstorage::IItem::Pointer>&);

 private:
  ListDirectoryModel list_;

  Q_OBJECT
};
#endif  // LISTDIRECTORYREQUEST_H
