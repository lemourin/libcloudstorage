#include "AndroidUtility.h"

#include <QAndroidJniObject>
#include <QtAndroid>

const int RECEIVER_CODE = 43;

void AndroidUtility::openWebPage(QString url) {
  if (intent_.isValid()) closeWebPage();
  intent_ = QAndroidJniObject::callStaticObjectMethod(
      "org/videolan/cloudbrowser/CloudBrowser", "openWebPage",
      "(Ljava/lang/String;)Landroid/content/Intent;",
      QAndroidJniObject::fromString(url).object());
  QtAndroid::startActivity(intent_, RECEIVER_CODE, &receiver_);
}

void AndroidUtility::closeWebPage() {
  QAndroidJniObject::callStaticObjectMethod(
      "org/videolan/cloudbrowser/CloudBrowser", "closeWebPage",
      "(Landroid/content/Intent;)V", intent_.object());
}

void AndroidUtility::ResultReceiver::handleActivityResult(
    int, int, const QAndroidJniObject &) {}
