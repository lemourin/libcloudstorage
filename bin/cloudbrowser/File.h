#ifndef FILE_H
#define FILE_H

#include <QFile>

#ifdef __ANDROID__

#include <QAndroidJniObject>

class AndroidFile : public QIODevice {
 public:
  AndroidFile(QString);
  virtual ~AndroidFile() = default;

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

#else

class DesktopFile : public QFile {
 public:
  DesktopFile(QString);
  virtual ~DesktopFile() = default;
};

using File = DesktopFile;
#endif  // __ANDROID__

#endif  // FILE_H
