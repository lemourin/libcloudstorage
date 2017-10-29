#include "FileDialog.h"

#include <QAndroidActivityResultReceiver>
#include <QAndroidJniObject>
#include <QtAndroid>

const int REQUEST_CODE = 42;

FileDialog::FileDialog() : result_receiver_(this) {}

void FileDialog::setSelectFolder(bool e) {
  if (select_folder_ == e) return;
  select_folder_ = e;
  emit selectFolderChanged();
}

void FileDialog::setSelectExisting(bool e) {
  if (select_existing_ == e) return;
  select_existing_ = e;
  emit selectExistingChanged();
}

void FileDialog::setFileUrl(QString e) {
  if (file_url_ == e) return;
  file_url_ = e;
  emit fileUrlChanged();
}

void FileDialog::setFilename(QString e) {
  if (filename_ == e) return;
  filename_ = e;
  emit filenameChanged();
}

void FileDialog::open() {
  QAndroidJniObject intent;
  if (selectFolder()) {
    intent = QAndroidJniObject::callStaticObjectMethod(
        "org/videolan/cloudbrowser/CloudBrowser", "createFileDialog",
        "(Ljava/lang/String;)Landroid/content/Intent;",
        QAndroidJniObject::fromString(filename()).object());
  } else {
    intent = QAndroidJniObject::callStaticObjectMethod(
        "org/videolan/cloudbrowser/CloudBrowser", "openFileDialog",
        "()Landroid/content/Intent;");
  }
  QtAndroid::startActivity(intent, REQUEST_CODE, &result_receiver_);
}

FileDialog::ActivityReceiver::ActivityReceiver(FileDialog *dialog)
    : file_dialog_(dialog) {}

void FileDialog::ActivityReceiver::handleActivityResult(
    int request_code, int result_code, const QAndroidJniObject &data) {
  if (request_code != REQUEST_CODE || result_code != -1) return;
  auto uri = data.callObjectMethod("getData", "()Landroid/net/Uri;");
  auto filename = QAndroidJniObject::callStaticObjectMethod(
      "org/videolan/cloudbrowser/CloudBrowser", "filename",
      "(Landroid/net/Uri;)Ljava/lang/String;", uri.object());
  file_dialog_->setFilename(filename.toString());
  file_dialog_->setFileUrl(uri.toString());
  emit file_dialog_->accepted();
}
