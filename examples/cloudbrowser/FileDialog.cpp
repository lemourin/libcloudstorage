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

void FileDialog::open() {
  QAndroidJniObject intent = QAndroidJniObject::callStaticObjectMethod(
      "org/videolan/cloudbrowser/CloudBrowser", "dialog",
      "(Z)Landroid/content/Intent;", selectFolder());
  QtAndroid::startActivity(intent, REQUEST_CODE, &result_receiver_);
}

FileDialog::ActivityReceiver::ActivityReceiver(FileDialog *dialog)
    : file_dialog_(dialog) {}

void FileDialog::ActivityReceiver::handleActivityResult(
    int request_code, int result_code, const QAndroidJniObject &data) {
  if (request_code != REQUEST_CODE || result_code != -1) return;
  auto uri = data.callObjectMethod("getData", "()Landroid/net/Uri;");
  auto real_url = QAndroidJniObject::callStaticObjectMethod(
                      "org/videolan/cloudbrowser/CloudBrowser", "getPath",
                      "(Landroid/net/Uri;)Ljava/lang/String;", uri.object())
                      .toString();
  file_dialog_->setFileUrl("file://" + real_url);
  emit file_dialog_->accepted();
}
