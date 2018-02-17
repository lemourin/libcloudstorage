#ifdef __ANDROID__

#include "AndroidUtility.h"

#include <QAndroidJniEnvironment>
#include <QAndroidJniObject>
#include <QDebug>
#include <QIcon>
#include <QUrl>
#include <set>

#include "CloudContext.h"
#include "Request/GetThumbnail.h"
#include "Utility/Utility.h"

const int RECEIVER_CODE = 43;

namespace {
AndroidUtility *android = nullptr;
QAndroidJniObject activity;
std::mutex mutex;
std::unordered_map<int, std::unordered_set<AndroidUtility::IResultListener *>>
    result_listener;
}  // namespace

extern "C" {
JNIEXPORT void JNICALL Java_org_videolan_cloudbrowser_Utility_onActionRequested(
    JNIEnv *env, jclass *, jstring action) {
  const char *str = env->GetStringUTFChars(action, nullptr);
  emit android->notify(str);
  env->ReleaseStringUTFChars(action, str);
}

JNIEXPORT void JNICALL Java_org_videolan_cloudbrowser_Utility_onRequestResult(
    JNIEnv *, jclass *, jint request, jint result, jobject data) {
  std::unique_lock<std::mutex> lock(mutex);
  auto cb = result_listener.find(request);
  if (cb != result_listener.end()) {
    while (!cb->second.empty()) {
      auto c = *cb->second.begin();
      cb->second.erase(cb->second.begin());
      lock.unlock();
      c->done(request, result, QAndroidJniObject(data));
      lock.lock();
    }
    result_listener.erase(cb);
  }
}
}

AndroidUtility::AndroidUtility() { android = this; }

AndroidUtility::~AndroidUtility() { android = nullptr; }

bool AndroidUtility::mobile() const { return true; }

QString AndroidUtility::name() const { return "android"; }

bool AndroidUtility::openWebPage(QString url) {
  if (intent_.isValid()) closeWebPage();
  intent_ = activity().callObjectMethod(
      "openWebPage", "(Ljava/lang/String;)Landroid/content/Intent;",
      QAndroidJniObject::fromString(url).object());
  startActivity(intent_, RECEIVER_CODE);
  return true;
}

void AndroidUtility::closeWebPage() {
  activity().callMethod<void>("closeWebPage", "(Landroid/content/Intent;)V",
                              intent_.object());
}

void AndroidUtility::landscapeOrientation() {
  activity().callMethod<void>("setLandScapeOrientation");
}

void AndroidUtility::defaultOrientation() {
  activity().callMethod<void>("setDefaultOrientation");
}

void AndroidUtility::showPlayerNotification(bool playing, QString filename,
                                            QString title) {
  auto arg0 = QAndroidJniObject::fromString(
      GetThumbnailRequest::thumbnail_path(filename));
  auto arg1 = QAndroidJniObject::fromString(filename);
  auto arg2 = QAndroidJniObject::fromString(title);
  auto notification = activity().callObjectMethod(
      "notification", "()Lorg/videolan/cloudbrowser/NotificationHelper;");
  notification.callMethod<void>(
      "showPlayerNotification",
      "(ZLjava/lang/String;Ljava/lang/String;Ljava/lang/String;)V", playing,
      arg0.object(), arg1.object(), arg2.object());
}

void AndroidUtility::hidePlayerNotification() {
  auto notification = activity().callObjectMethod(
      "notification", "()Lorg/videolan/cloudbrowser/NotificationHelper;");
  notification.callMethod<void>("hidePlayerNotification");
}

void AndroidUtility::enableKeepScreenOn() {
  activity().callMethod<void>("enableKeepScreenOn");
}

void AndroidUtility::disableKeepScreenOn() {
  activity().callMethod<void>("disableKeepScreenOn");
}

void AndroidUtility::showAd() { activity().callMethod<void>("showAd"); }

void AndroidUtility::hideAd() { activity().callMethod<void>("hideAd"); }

void AndroidUtility::setActivity(const QAndroidJniObject &activity) {
  ::activity = activity;
}

QAndroidJniObject AndroidUtility::activity() { return ::activity; }

void AndroidUtility::startActivity(const QAndroidJniObject &intent,
                                   int request_code,
                                   const IResultListener *listener) {
  if (listener) {
    listener->code_ = request_code;
    std::unique_lock<std::mutex> lock(mutex);
    result_listener[request_code].insert(
        const_cast<IResultListener *>(listener));
    lock.unlock();
    activity().callMethod<void>("startActivityForResult",
                                "(Landroid/content/Intent;I)V", intent.object(),
                                request_code);
  } else {
    activity().callMethod<void>("startActivity", "(Landroid/content/Intent;)V",
                                intent.object());
  }
}

IPlatformUtility::Pointer IPlatformUtility::create() {
  return cloudstorage::util::make_unique<AndroidUtility>();
}

AndroidUtility::IResultListener::~IResultListener() {
  std::unique_lock<std::mutex> lock(mutex);
  auto it = result_listener.find(code_);
  if (it != result_listener.end()) {
    auto e = it->second.find(this);
    if (e != it->second.end()) it->second.erase(e);
    if (it->second.empty()) result_listener.erase(it);
  }
}

#endif  // __ANDROID__
