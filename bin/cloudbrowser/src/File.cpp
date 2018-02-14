#include "File.h"

#include <QUrl>

#ifdef __ANDROID__

#include <QAndroidJniEnvironment>
#include <QtAndroid>

AndroidFile::AndroidFile(QString uri) : uri_(uri) {}

bool AndroidFile::open(QIODevice::OpenMode mode) {
  QAndroidJniEnvironment env;
  if (mode == QIODevice::WriteOnly) {
    file_ = QAndroidJniObject("org/videolan/cloudbrowser/FileOutput",
                              "(Landroid/content/Context;Ljava/lang/String;)V",
                              QtAndroid::androidActivity().object(),
                              QAndroidJniObject::fromString(uri_).object());
    if (env->ExceptionCheck()) {
      env->ExceptionDescribe();
      env->ExceptionClear();
      return false;
    }
    setOpenMode(mode);
    return true;
  } else if (mode == QIODevice::ReadOnly) {
    file_ = QAndroidJniObject("org/videolan/cloudbrowser/FileInput",
                              "(Landroid/content/Context;Ljava/lang/String;)V",
                              QtAndroid::androidActivity().object(),
                              QAndroidJniObject::fromString(uri_).object());
    if (env->ExceptionCheck()) {
      env->ExceptionDescribe();
      env->ExceptionClear();
      return false;
    }
    setOpenMode(mode);
    return true;
  }
  return false;
}

void AndroidFile::close() {
  if (!isOpen()) return;
  file_.callMethod<void>("close");
  setOpenMode(NotOpen);
}

qint64 AndroidFile::readData(char *buffer, qint64 size) {
  if (!isReadable()) return 0;
  auto d = file_.callObjectMethod("read", "(I)[B", static_cast<int>(size));
  QAndroidJniEnvironment env;
  auto ret = env->GetArrayLength(d.object<jbyteArray>());
  env->GetByteArrayRegion(d.object<jbyteArray>(), 0, ret,
                          reinterpret_cast<jbyte *>(buffer));
  return ret;
}

qint64 AndroidFile::size() const {
  if (!isReadable()) return 0;
  return file_.callMethod<jlong>("size", "()J");
}

bool AndroidFile::reset() {
  if (!isOpen()) return false;
  return file_.callMethod<jboolean>("reset", "()Z");
}

qint64 AndroidFile::writeData(const char *data, qint64 max_size) {
  if (!isWritable()) return 0;
  QAndroidJniEnvironment env;
  auto array = env->NewByteArray(max_size);
  env->SetByteArrayRegion(array, 0, max_size,
                          reinterpret_cast<const jbyte *>(data));
  auto d = QAndroidJniObject::fromLocalRef(array);
  file_.callMethod<void>("write", "([B)V", d.object());
  if (env->ExceptionCheck()) {
    env->ExceptionDescribe();
    env->ExceptionClear();
    return 0;
  }
  return max_size;
}

#endif  // __ANDROID__

#ifdef WINRT

#include <ppltasks.h>
#include <private/qeventdispatcher_winrt_p.h>
#include <QDebug>
#include <future>

using namespace Windows::Storage;
using namespace Windows::Storage::FileProperties;
using namespace Windows::Storage::Streams;
using namespace Platform;
using namespace concurrency;

std::mutex WinRTFile::mutex_;
std::map<QString, StorageFile ^> WinRTFile::registered_file_;

WinRTFile::WinRTFile(QString path) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = registered_file_.find(path);
  if (it != registered_file_.end()) {
    file_ = it->second;
    registered_file_.erase(it);
  }
}

bool WinRTFile::open(OpenMode mode) {
  std::promise<bool> result;
  if (mode == QIODevice::WriteOnly) {
    QEventDispatcherWinRT::runOnXamlThread([&]() {
      create_task(file_->OpenTransactedWriteAsync())
          .then([&](StorageStreamTransaction ^ t) {
            write_stream_ = t;
            writer_ = ref new DataWriter(t->Stream);
            result.set_value(true);
          });
      return true;
    });
    setOpenMode(mode);
  } else if (mode == QIODevice::ReadOnly) {
    QEventDispatcherWinRT::runOnXamlThread([&]() {
      create_task(file_->OpenSequentialReadAsync())
          .then([&](IInputStream ^ stream) {
            read_stream_ = stream;
            reader_ = ref new DataReader(stream);
            result.set_value(true);
          });
      return true;
    });
    setOpenMode(mode);
  } else {
    result.set_value(false);
  }
  return result.get_future().get();
}

void WinRTFile::close() {
  std::promise<void> e;
  QEventDispatcherWinRT::runOnXamlThread([&]() {
    if (write_stream_)
      create_task(write_stream_->CommitAsync()).then([&]() { e.set_value(); });
    return true;
  });
  e.get_future().get();
}

qint64 WinRTFile::size() const {
  std::promise<qint64> result;
  QEventDispatcherWinRT::runOnXamlThread([&]() {
    create_task(file_->GetBasicPropertiesAsync())
        .then([&](BasicProperties ^ p) { result.set_value(p->Size); });
    return true;
  });
  return result.get_future().get();
}

bool WinRTFile::reset() { return open(openMode()); }

void WinRTFile::register_file(QString path,
                              Windows::Storage::StorageFile ^ file) {
  std::lock_guard<std::mutex> lock(mutex_);
  registered_file_[path] = file;
}

qint64 WinRTFile::writeData(const char *data, qint64 max_size) {
  std::promise<qint64> result;
  QEventDispatcherWinRT::runOnXamlThread([&]() {
    auto bytes = ref new Array<uchar>(
        const_cast<uchar *>(reinterpret_cast<const uchar *>(data)), max_size);
    writer_->WriteBytes(bytes);
    create_task(writer_->StoreAsync()).then([&](unsigned int bytes_written) {
      result.set_value(bytes_written);
    });
    return true;
  });
  return result.get_future().get();
}

qint64 WinRTFile::readData(char *data, qint64 max_size) {
  std::promise<qint64> result;
  QEventDispatcherWinRT::runOnXamlThread([&]() {
    create_task(reader_->LoadAsync(max_size))
        .then([&](unsigned int bytes_read) {
          auto arr = ref new Array<uchar>(bytes_read);
          reader_->ReadBytes(arr);
          memcpy(data, arr->Data, bytes_read);
          result.set_value(bytes_read);
        });
    return true;
  });
  return result.get_future().get();
}

#endif  // WINRT

#ifndef WINRT
#ifndef __ANDROID__

DesktopFile::DesktopFile(QString path) : QFile(QUrl(path).toLocalFile()) {}

#endif
#endif
