#ifndef FILEDIALOG_H
#define FILEDIALOG_H

#ifdef __ANDROID__

#include <QAndroidActivityResultReceiver>
#include <QAndroidJniObject>
#include <QDebug>
#include <QObject>
#include <QtAndroid>

class FileDialog : public QObject {
 public:
  Q_PROPERTY(bool selectFolder READ selectFolder WRITE setSelectFolder NOTIFY
                 selectFolderChanged)
  Q_PROPERTY(bool selectExisting READ selectExisting WRITE setSelectExisting
                 NOTIFY selectExistingChanged)
  Q_PROPERTY(QString fileUrl READ fileUrl NOTIFY fileUrlChanged)
  Q_PROPERTY(
      QString filename READ filename WRITE setFilename NOTIFY filenameChanged)

  FileDialog();

  bool selectFolder() const { return select_folder_; }
  void setSelectFolder(bool);

  bool selectExisting() const { return select_existing_; }
  void setSelectExisting(bool);

  QString fileUrl() const { return file_url_; }
  void setFileUrl(QString);

  QString filename() const { return filename_; }
  void setFilename(QString);

  Q_INVOKABLE void open();

 signals:
  void accepted();
  void selectFolderChanged();
  void selectExistingChanged();
  void fileUrlChanged();
  void filenameChanged();

 private:
  class ActivityReceiver : public QAndroidActivityResultReceiver {
   public:
    ActivityReceiver(FileDialog*);
    void handleActivityResult(int request_code, int result_code,
                              const QAndroidJniObject& data) override;

   private:
    FileDialog* file_dialog_;
  } result_receiver_;
  bool select_folder_;
  bool select_existing_;
  QString file_url_;
  QString filename_;

  Q_OBJECT
};

#endif  // __ANDROID__
#endif  // FILEDIALOG_H
