#include "FileDialog.h"
#include "File.h"

IFileDialog::IFileDialog() : select_existing_() {}

void IFileDialog::setSelectExisting(bool e) {
  if (select_existing_ == e) return;
  select_existing_ = e;
  emit selectExistingChanged();
}

void IFileDialog::setUrl(QString e) {
  if (url_ == e) return;
  url_ = e;
  emit urlChanged();
}

void IFileDialog::setFilename(QString e) {
  if (filename_ == e) return;
  filename_ = e;
  emit filenameChanged();
}

#ifdef __ANDROID__

#include <QAndroidActivityResultReceiver>
#include <QAndroidJniObject>
#include "AndroidUtility.h"

const int REQUEST_CODE = 42;

FileDialog::FileDialog() : result_receiver_(this) {}

void FileDialog::open() {
  QAndroidJniObject intent;
  if (!selectExisting()) {
    intent = QAndroidJniObject::callStaticObjectMethod(
        "org/videolan/cloudbrowser/Utility", "createFileDialog",
        "(Ljava/lang/String;)Landroid/content/Intent;",
        QAndroidJniObject::fromString(filename()).object());
  } else {
    intent = QAndroidJniObject::callStaticObjectMethod(
        "org/videolan/cloudbrowser/Utility", "openFileDialog",
        "()Landroid/content/Intent;");
  }
  AndroidUtility::startActivity(intent.object(), REQUEST_CODE,
                                &result_receiver_);
}

FileDialog::ActivityReceiver::ActivityReceiver(FileDialog *dialog)
    : file_dialog_(dialog) {}

void FileDialog::ActivityReceiver::done(int request_code, int result_code,
                                        const QAndroidJniObject &data) {
  if (request_code != REQUEST_CODE || result_code != -1) return;
  auto uri = data.callObjectMethod("getData", "()Landroid/net/Uri;");
  auto filename = AndroidUtility::activity().callObjectMethod(
      "filename", "(Landroid/net/Uri;)Ljava/lang/String;", uri.object());
  file_dialog_->setFilename(filename.toString());
  file_dialog_->setUrl(uri.toString());
  emit file_dialog_->ready();
}

#endif  // __ANDROID__

#ifdef WINRT

#include <collection.h>
#include <ppltasks.h>
#include <private/qeventdispatcher_winrt_p.h>
#include <qfunctions_winrt.h>
#include <QDebug>
#include <QUrl>

using namespace concurrency;
using namespace Windows::Storage;
using namespace Windows::Storage::Pickers;
using namespace Platform;
using namespace Platform::Collections;

void FileDialog::open() {
  QEventDispatcherWinRT::runOnXamlThread([this]() {
    if (!selectExisting()) {
      auto picker = ref new FileSavePicker();
      picker->SuggestedFileName =
          ref new String(filename().toStdWString().c_str());
      auto idx = filename().lastIndexOf('.');
      auto ext = idx != -1 ? filename().mid(idx) : "";
      auto extensions = ref new Collections::Vector<String ^>();
      extensions->Append(ref new String(ext.toStdWString().c_str()));
      picker->FileTypeChoices->Insert("", extensions);
      create_task(picker->PickSaveFileAsync()).then([this](StorageFile ^ file) {
        if (file) {
          auto path =
              QUrl::fromLocalFile(QString::fromWCharArray(file->Path->Data()))
                  .toString();
          WinRTFile::register_file(path, file);
          setUrl(path);
          emit ready();
        }
      });
    } else {
      auto picker = ref new FileOpenPicker();
      picker->FileTypeFilter->Append("*");
      create_task(picker->PickSingleFileAsync())
          .then([this](StorageFile ^ file) {
            if (file) {
              auto path = QUrl::fromLocalFile(
                              QString::fromWCharArray(file->Path->Data()))
                              .toString();
              WinRTFile::register_file(path, file);
              setFilename(path.mid(path.lastIndexOf('/') + 1));
              setUrl(path);
              emit ready();
            }
          });
    }
    return true;
  });
}

#endif  // WINRT
