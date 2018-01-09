#include "DesktopUtility.h"

#ifndef __ANDROID__

#include <QDesktopServices>
#include <QUrl>

#include "Utility/Utility.h"

bool DesktopUtility::mobile() const { return false; }

QString DesktopUtility::name() const { return "desktop"; }

bool DesktopUtility::openWebPage(QString url) {
  return QDesktopServices::openUrl(url);
}

void DesktopUtility::closeWebPage() {}

void DesktopUtility::landscapeOrientation() {}

void DesktopUtility::defaultOrientation() {}

void DesktopUtility::showPlayerNotification(bool, QString, QString) {}

void DesktopUtility::hidePlayerNotification() {}

void DesktopUtility::enableKeepScreenOn() {}

void DesktopUtility::disableKeepScreenOn() {}

IPlatformUtility::Pointer IPlatformUtility::create() {
  return cloudstorage::util::make_unique<DesktopUtility>();
}

#endif // __ANDROID__
