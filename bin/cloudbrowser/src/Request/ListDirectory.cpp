#include "ListDirectory.h"

#include <QQmlEngine>
#include "CloudContext.h"
#include "Utility/Utility.h"

using namespace cloudstorage;

namespace {
class ListDirectory : public IListDirectoryCallback {
 public:
  ListDirectory(RequestNotifier* r) : notifier_(r) {}

  void receivedItem(IItem::Pointer item) override {
    notifier_->addedItem(item);
  }

  void done(EitherError<IItem::List> r) override {
    emit notifier_->finishedList(r);
    notifier_->deleteLater();
  }

 private:
  RequestNotifier* notifier_;
};
}  // namespace

void ListDirectoryModel::set_provider(const Provider& p) { provider_ = p; }

void ListDirectoryModel::add(IItem::Pointer t) {
  beginInsertRows(QModelIndex(), rowCount(), rowCount());
  list_.push_back(t);
  id_.insert(t->id());
  endInsertRows();
}

void ListDirectoryModel::clear() {
  beginRemoveRows(QModelIndex(), 0, std::max<int>(rowCount() - 1, 0));
  list_.clear();
  id_.clear();
  endRemoveRows();
}

int ListDirectoryModel::find(IItem::Pointer item) const {
  if (id_.find(item->id()) == std::end(id_)) return -1;
  for (size_t i = 0; i < list_.size(); i++)
    if (list_[i]->id() == item->id()) return i;
  return -1;
}

void ListDirectoryModel::insert(int idx, IItem::Pointer item) {
  beginInsertRows(QModelIndex(), idx, idx);
  list_.insert(list_.begin() + idx, item);
  id_.insert(item->id());
  endInsertRows();
}

void ListDirectoryModel::remove(int idx) {
  beginRemoveRows(QModelIndex(), idx, idx);
  id_.erase(id_.find(list_[idx]->id()));
  list_.erase(list_.begin() + idx);
  endRemoveRows();
}

void ListDirectoryModel::assign(const IItem::List& lst) {
  std::unordered_set<std::string> id;
  for (size_t i = lst.size(); i-- > 0;) {
    auto idx = find(lst[i]);
    if (idx == -1) {
      beginInsertRows(QModelIndex(), 0, 0);
      list_.insert(list_.begin(), lst[i]);
      id_.insert(lst[i]->id());
      endInsertRows();
    } else {
      list_[idx] = lst[i];
      emit dataChanged(createIndex(idx, 0), createIndex(idx, 0));
    }
    id.insert(lst[i]->id());
  }
  for (size_t i = 0; i < list().size();) {
    if (id.find(list()[i]->id()) == std::end(id)) {
      remove(i);
    } else {
      i++;
    }
  }
}

int ListDirectoryModel::rowCount(const QModelIndex&) const {
  return static_cast<int>(list_.size());
}

QVariant ListDirectoryModel::data(const QModelIndex& id, int) const {
  if (static_cast<uint32_t>(id.row()) >= list_.size()) return QVariant();
  auto item = new CloudItem(provider_, list_[static_cast<size_t>(id.row())]);
  QQmlEngine::setObjectOwnership(item, QQmlEngine::JavaScriptOwnership);
  return QVariant::fromValue(item);
}

QHash<int, QByteArray> ListDirectoryModel::roleNames() const {
  return {{Qt::DisplayRole, "modelData"}};
}

ListDirectoryModel* ListDirectoryRequest::list() { return &list_; }

void ListDirectoryRequest::update(CloudContext* context, CloudItem* item) {
  list_.set_provider(item->provider());

  auto cache_key = item->key();
  auto cached = context->cachedDirectory(cache_key);
  bool cache_found = !cached.empty();
  if (!cached.empty()) {
    list_.assign(cached);
  }
  set_done(false);

  connect(this, &Request::errorOccurred, context,
          [=](QVariantMap provider, int code, QString description) {
            emit context->errorOccurred("ListDirectory", provider, code,
                                        description);
          });
  connect(this, &ListDirectoryRequest::finished, context,
          [=](ListDirectoryCacheKey key, const IItem::List& r) {
            context->cacheDirectory(key, r);
          });

  auto object = new RequestNotifier;
  auto provider = item->provider().variant();
  connect(object, &RequestNotifier::finishedList, this,
          [=](EitherError<IItem::List> r) {
            if (r.left()) {
              set_done(true);
              emit errorOccurred(provider, r.left()->code_,
                                 r.left()->description_.c_str());
            } else {
              if (cache_found) list_.assign(*r.right());
              emit finished(cache_key, *r.right());
              set_done(true);
            }
          });
  connect(object, &RequestNotifier::addedItem, this, [=](IItem::Pointer item) {
    if (!cache_found) list_.add(item);
  });
  auto r = item->provider().provider_->listDirectoryAsync(
      item->item(), util::make_unique<ListDirectory>(object));
  context->add(item->provider().provider_, std::move(r));
}
