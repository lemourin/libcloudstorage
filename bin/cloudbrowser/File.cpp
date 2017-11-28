#include "File.h"

#include <QUrl>

#ifdef __ANDROID__

#include <QAndroidJniEnvironment>

AndroidFile::AndroidFile(QString uri) : uri_(uri) {}

bool AndroidFile::open(QIODevice::OpenMode mode) {
  QAndroidJniEnvironment env;
  if (mode == QIODevice::WriteOnly) {
    file_ = QAndroidJniObject::callStaticObjectMethod(
        "org/videolan/cloudbrowser/CloudBrowser", "openWrite",
        "(Ljava/lang/String;)Lorg/videolan/cloudbrowser/"
        "CloudBrowser$FileOutput;",
        QAndroidJniObject::fromString(uri_).object());
    if (env->ExceptionCheck()) {
      env->ExceptionDescribe();
      env->ExceptionClear();
      return false;
    }
    setOpenMode(mode);
    return true;
  } else if (mode == QIODevice::ReadOnly) {
    file_ = QAndroidJniObject::callStaticObjectMethod(
        "org/videolan/cloudbrowser/CloudBrowser", "openRead",
        "(Ljava/lang/String;)Lorg/videolan/cloudbrowser/"
        "CloudBrowser$FileInput;",
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
  if (openMode() == QIODevice::WriteOnly)
    QAndroidJniObject::callStaticMethod<void>(
        "org/videolan/cloudbrowser/CloudBrowser", "closeWrite",
        "(Lorg/videolan/cloudbrowser/CloudBrowser$FileOutput;)V",
        file_.object());
  else
    QAndroidJniObject::callStaticMethod<void>(
        "org/videolan/cloudbrowser/CloudBrowser", "closeRead",
        "(Lorg/videolan/cloudbrowser/CloudBrowser$FileInput;)V",
        file_.object());
  setOpenMode(NotOpen);
}

qint64 AndroidFile::readData(char *buffer, qint64 size) {
  if (!isReadable()) return 0;
  auto d = QAndroidJniObject::callStaticObjectMethod(
      "org/videolan/cloudbrowser/CloudBrowser", "read",
      "(Lorg/videolan/cloudbrowser/CloudBrowser$FileInput;I)[B", file_.object(),
      static_cast<int>(size));
  QAndroidJniEnvironment env;
  auto ret = env->GetArrayLength(d.object<jbyteArray>());
  env->GetByteArrayRegion(d.object<jbyteArray>(), 0, ret,
                          reinterpret_cast<jbyte *>(buffer));
  return ret;
}

qint64 AndroidFile::size() const {
  if (!isReadable()) return 0;
  return QAndroidJniObject::callStaticMethod<jlong>(
      "org/videolan/cloudbrowser/CloudBrowser", "size",
      "(Lorg/videolan/cloudbrowser/CloudBrowser$FileInput;)J", file_.object());
}

bool AndroidFile::reset() {
  if (!isOpen()) return false;
  return QAndroidJniObject::callStaticMethod<jboolean>(
      "org/videolan/cloudbrowser/CloudBrowser", "reset",
      "(Lorg/videolan/cloudbrowser/CloudBrowser$FileInput;)Z", file_.object());
}

qint64 AndroidFile::writeData(const char *data, qint64 max_size) {
  if (!isWritable()) return 0;
  QAndroidJniEnvironment env;
  auto array = env->NewByteArray(max_size);
  env->SetByteArrayRegion(array, 0, max_size,
                          reinterpret_cast<const jbyte *>(data));
  auto d = QAndroidJniObject::fromLocalRef(array);
  QAndroidJniObject::callStaticMethod<void>(
      "org/videolan/cloudbrowser/CloudBrowser", "write",
      "(Lorg/videolan/cloudbrowser/CloudBrowser$FileOutput;[B)V",
      file_.object(), d.object());
  if (env->ExceptionCheck()) {
    env->ExceptionDescribe();
    env->ExceptionClear();
    return 0;
  }
  return max_size;
}

#else

DesktopFile::DesktopFile(QString path) : QFile(QUrl(path).toLocalFile()) {}

#endif  // __ANDROID__
