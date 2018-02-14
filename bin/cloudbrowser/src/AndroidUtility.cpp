#ifdef __ANDROID__

#include "AndroidUtility.h"

#include <QAndroidJniEnvironment>
#include <QAndroidJniObject>
#include <QDebug>
#include <QIcon>
#include <QUrl>
#include <QtAndroid>

#include "CloudContext.h"
#include "Request/GetThumbnail.h"
#include "Utility/Utility.h"

const int RECEIVER_CODE = 43;

namespace {
AndroidUtility *android = nullptr;
}  // namespace

extern "C" {
JNIEXPORT void JNICALL
Java_org_videolan_cloudbrowser_NotificationHelper_callback(JNIEnv *env,
                                                           jclass *,
                                                           jstring action) {
  const char *str = env->GetStringUTFChars(action, nullptr);
  emit android->notify(str);
  env->ReleaseStringUTFChars(action, str);
}
}

AndroidUtility::AndroidUtility() { android = this; }

AndroidUtility::~AndroidUtility() { android = nullptr; }

bool AndroidUtility::mobile() const { return true; }

QString AndroidUtility::name() const { return "android"; }

bool AndroidUtility::openWebPage(QString url) {
  if (intent_.isValid()) closeWebPage();
  intent_ = QtAndroid::androidActivity().callObjectMethod(
      "openWebPage", "(Ljava/lang/String;)Landroid/content/Intent;",
      QAndroidJniObject::fromString(url).object());
  QtAndroid::startActivity(intent_, RECEIVER_CODE, &receiver_);
  return true;
}

void AndroidUtility::closeWebPage() {
  QtAndroid::androidActivity().callMethod<void>(
      "closeWebPage", "(Landroid/content/Intent;)V", intent_.object());
}

void AndroidUtility::landscapeOrientation() {
  QtAndroid::androidActivity().callMethod<void>("setLandScapeOrientation");
}

void AndroidUtility::defaultOrientation() {
  QtAndroid::androidActivity().callMethod<void>("setDefaultOrientation");
}

void AndroidUtility::showPlayerNotification(bool playing, QString filename,
                                            QString title) {
  auto arg0 = QAndroidJniObject::fromString(
      GetThumbnailRequest::thumbnail_path(filename));
  auto arg1 = QAndroidJniObject::fromString(filename);
  auto arg2 = QAndroidJniObject::fromString(title);
  auto notification = QtAndroid::androidContext().callObjectMethod(
      "notification", "()Lorg/videolan/cloudbrowser/NotificationHelper;");
  notification.callMethod<void>(
      "showPlayerNotification",
      "(ZLjava/lang/String;Ljava/lang/String;Ljava/lang/String;)V", playing,
      arg0.object(), arg1.object(), arg2.object());
}

void AndroidUtility::hidePlayerNotification() {
  auto notification = QtAndroid::androidContext().callObjectMethod(
      "notification", "()Lorg/videolan/cloudbrowser/NotificationHelper;");
  notification.callMethod<void>("hidePlayerNotification");
}

void AndroidUtility::enableKeepScreenOn() {
  QtAndroid::androidActivity().callMethod<void>("enableKeepScreenOn");
}

void AndroidUtility::disableKeepScreenOn() {
  QtAndroid::androidActivity().callMethod<void>("disableKeepScreenOn");
}

void AndroidUtility::showAd() {
  QtAndroid::androidActivity().callMethod<void>("showAd");
}

void AndroidUtility::hideAd() {
  QtAndroid::androidActivity().callMethod<void>("hideAd");
}

void AndroidUtility::ResultReceiver::handleActivityResult(
    int, int, const QAndroidJniObject &) {}

IPlatformUtility::Pointer IPlatformUtility::create() {
  return cloudstorage::util::make_unique<AndroidUtility>();
}

#endif  // __ANDROID__
