#include "IOSUtility.h"

#ifdef IOS

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#include "Utility/Utility.h"

#include <QDesktopServices>
#include <QUrl>

void IOSUtility::initialize(QWindow *) const {}

bool IOSUtility::mobile() const { return true; }

QString IOSUtility::name() const { return "ios"; }

bool IOSUtility::openWebPage(QString url) {
  return QDesktopServices::openUrl(url);
}

void IOSUtility::closeWebPage() {
}

void IOSUtility::landscapeOrientation() {
}

void IOSUtility::defaultOrientation() {
}

void IOSUtility::showPlayerNotification(bool playing, QString filename,
                                        QString title) {
}

void IOSUtility::hidePlayerNotification() {
}

void IOSUtility::enableKeepScreenOn() {
  [[UIApplication sharedApplication] setIdleTimerDisabled: YES];
}

void IOSUtility::disableKeepScreenOn() {
  [[UIApplication sharedApplication] setIdleTimerDisabled: NO];
}

void IOSUtility::showAd() {}

void IOSUtility::hideAd() {}

IPlatformUtility::Pointer IPlatformUtility::create() {
  return cloudstorage::util::make_unique<IOSUtility>();
}

#endif  //  IOS
