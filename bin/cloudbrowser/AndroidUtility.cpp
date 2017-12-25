#ifdef __ANDROID__

#include "AndroidUtility.h"

#include <QAndroidJniEnvironment>
#include <QAndroidJniObject>
#include <QDebug>
#include <QIcon>
#include <QUrl>
#include <QtAndroid>

#include "CloudContext.h"

const int RECEIVER_CODE = 43;

namespace {
AndroidUtility *android = nullptr;
}  // namespace

extern "C" {
JNIEXPORT void JNICALL
Java_org_videolan_cloudbrowser_NotificationHelper_callback(JNIEnv *env, jclass *,
                                                           jstring action) {
  const char* str = env->GetStringUTFChars(action, nullptr);
  emit android->notify(str);
  env->ReleaseStringUTFChars(action, str);
}
}

AndroidUtility::AndroidUtility() { android = this; }

AndroidUtility::~AndroidUtility() { android = nullptr; }

void AndroidUtility::openWebPage(QString url) {
  if (intent_.isValid()) closeWebPage();
  intent_ = QAndroidJniObject::callStaticObjectMethod(
      "org/videolan/cloudbrowser/CloudBrowser", "openWebPage",
      "(Ljava/lang/String;)Landroid/content/Intent;",
      QAndroidJniObject::fromString(url).object());
  QtAndroid::startActivity(intent_, RECEIVER_CODE, &receiver_);
}

void AndroidUtility::closeWebPage() {
  QAndroidJniObject::callStaticMethod<void>(
      "org/videolan/cloudbrowser/CloudBrowser", "closeWebPage",
      "(Landroid/content/Intent;)V", intent_.object());
}

void AndroidUtility::landscapeOrientation() {
  QAndroidJniObject::callStaticMethod<void>(
      "org/videolan/cloudbrowser/CloudBrowser", "setLandScapeOrientation",
      "()V");
}

void AndroidUtility::defaultOrientation() {
  QAndroidJniObject::callStaticMethod<void>(
      "org/videolan/cloudbrowser/CloudBrowser", "setDefaultOrientation", "()V");
}

void AndroidUtility::showPlayerNotification(bool playing, QString filename,
                                            QString title) {
  auto arg0 =
      QAndroidJniObject::fromString(CloudContext::thumbnail_path(filename));
  auto arg1 = QAndroidJniObject::fromString(filename);
  auto arg2 = QAndroidJniObject::fromString(title);
  QAndroidJniObject::callStaticMethod<void>(
      "org/videolan/cloudbrowser/NotificationHelper", "showPlayerNotification",
      "(ZLjava/lang/String;Ljava/lang/String;Ljava/lang/String;)V", playing,
      arg0.object(), arg1.object(), arg2.object());
}

void AndroidUtility::hidePlayerNotification() {
  QAndroidJniObject::callStaticMethod<void>(
      "org/videolan/cloudbrowser/NotificationHelper", "hidePlayerNotification",
      "()V");
}

void AndroidUtility::ResultReceiver::handleActivityResult(
    int, int, const QAndroidJniObject &) {}

#endif  // __ANDROID__
