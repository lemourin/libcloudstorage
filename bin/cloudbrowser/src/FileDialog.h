#ifndef FILEDIALOG_H
#define FILEDIALOG_H

#include <QObject>
#include <QString>

#include "WinRTUtility.h"

class IFileDialog : public QObject {
 public:
  Q_PROPERTY(bool selectExisting READ selectExisting WRITE setSelectExisting
                 NOTIFY selectExistingChanged)
  Q_PROPERTY(
      QString filename READ filename WRITE setFilename NOTIFY filenameChanged)
  Q_PROPERTY(QString url READ url NOTIFY urlChanged)

  IFileDialog();

  Q_INVOKABLE virtual void open() = 0;

  QString filename() const { return filename_; }
  void setFilename(QString);

  bool selectExisting() const { return select_existing_; }
  void setSelectExisting(bool);

  QString url() const { return url_; }
  void setUrl(QString);

 signals:
  void ready();
  void selectExistingChanged();
  void urlChanged();
  void filenameChanged();

 private:
  bool select_existing_;
  QString url_;
  QString filename_;

  Q_OBJECT
};

#ifdef __ANDROID__

#include <QAndroidJniObject>
#include "AndroidUtility.h"

class FileDialog : public IFileDialog {
 public:
  FileDialog();

  void open() override;

 private:
  class ActivityReceiver : public AndroidUtility::IResultListener {
   public:
    ActivityReceiver(FileDialog*);
    void done(int request_code, int result_code,
              const QAndroidJniObject& data) override;

   private:
    FileDialog* file_dialog_;
  } result_receiver_;
};

#endif  // __ANDROID__

#ifdef WINRT

class FileDialog : public IFileDialog {
 public:
  Q_INVOKABLE void open() override;
};

#endif  // WINRT

#endif  // FILEDIALOG_H
