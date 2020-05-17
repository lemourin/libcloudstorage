#ifndef REQUEST_H
#define REQUEST_H

#include <QObject>
#include <QVariant>

#include "Exec.h"
#include "ICloudProvider.h"

class RequestNotifier : public QObject {
 signals:
  void addedItem(cloudstorage::IItem::Pointer);
  void finishedList(
      cloudstorage::EitherError<std::vector<cloudstorage::IItem::Pointer>>);
  void finishedVoid(cloudstorage::EitherError<void>);
  void finishedString(cloudstorage::EitherError<std::string>);
  void finishedItem(cloudstorage::EitherError<cloudstorage::IItem>);
  void finishedVariant(cloudstorage::EitherError<QVariant>);
  void progressChanged(qint64 total, qint64 now);

 private:
  Q_OBJECT
};

class Request : public QObject {
 public:
  Q_PROPERTY(bool done READ done WRITE set_done NOTIFY doneChanged)

  bool done() const { return done_; }
  void set_done(bool);

 signals:
  void doneChanged();
  void errorOccurred(QVariantMap provider, int code, QString description);

 private:
  bool done_ = false;

  Q_OBJECT
};

Q_DECLARE_METATYPE(cloudstorage::IItem::Pointer)
Q_DECLARE_METATYPE(
    cloudstorage::EitherError<std::vector<cloudstorage::IItem::Pointer>>)
Q_DECLARE_METATYPE(cloudstorage::EitherError<void>)
Q_DECLARE_METATYPE(cloudstorage::EitherError<std::string>)
Q_DECLARE_METATYPE(cloudstorage::EitherError<cloudstorage::IItem>)
Q_DECLARE_METATYPE(cloudstorage::EitherError<QVariant>)

#endif  // REQUEST_H
