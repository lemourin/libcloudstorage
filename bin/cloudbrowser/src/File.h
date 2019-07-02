#ifndef FILE_H
#define FILE_H

#include <QFile>
#include <mutex>
#include "WinRTUtility.h"

#ifdef __ANDROID__

#include <QAndroidJniObject>

class AndroidFile : public QIODevice {
 public:
  AndroidFile(const QString &);

  bool open(OpenMode) override;
  void close() override;
  qint64 size() const override;
  bool reset() override;

 protected:
  qint64 writeData(const char *data, qint64 max_size) override;
  qint64 readData(char *, qint64) override;

 private:
  QString uri_;
  QAndroidJniObject file_;
};

using File = AndroidFile;

#endif  // __ANDROID__

#ifdef WINRT

class WinRTFile : public QIODevice {
 public:
  WinRTFile(const QString &);

  bool open(OpenMode) override;
  void close() override;
  qint64 size() const override;
  bool reset() override;

  static void register_file(QString, Windows::Storage::StorageFile ^);

 protected:
  qint64 writeData(const char *data, qint64 max_size) override;
  qint64 readData(char *, qint64) override;

 private:
  static std::mutex mutex_;
  static std::map<QString, Windows::Storage::StorageFile ^> registered_file_;
  Windows::Storage::StorageFile ^ file_;
  Windows::Storage::StorageStreamTransaction ^ write_stream_;
  Windows::Storage::Streams::DataWriter ^ writer_;
  Windows::Storage::Streams::IInputStream ^ read_stream_;
  Windows::Storage::Streams::DataReader ^ reader_;
};

using File = WinRTFile;

#endif  //  WINRT

#ifndef WINRT
#ifndef __ANDROID__

class DesktopFile : public QFile {
 public:
  DesktopFile(const QString &);
};

using File = DesktopFile;
#endif
#endif

#endif  // FILE_H
