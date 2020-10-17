#include "WinRTUtility.h"

#ifdef WINRT

#include <private/qeventdispatcher_winrt_p.h>
#include <QDebug>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QUrl>
#include <QWindow>

#include "Utility/Utility.h"

#include <windows.ui.h>
#include <wrl.h>

using namespace Microsoft::WRL;
using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::System::Display;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;

WinRTUtility::WinRTUtility() {
  QEventDispatcherWinRT::runOnXamlThread([this]() {
    display_request_ = ref new DisplayRequest;
    return true;
  });
}

void WinRTUtility::initialize(QWindow* window) const {
  window->setFlag(Qt::MaximizeUsingFullscreenGeometryHint);
  window->setGeometry(0, 0, 0, 0);
}

bool WinRTUtility::mobile() const { return false; }

QString WinRTUtility::name() const { return "winrt"; }

bool WinRTUtility::openWebPage(QString url) {
  return QDesktopServices::openUrl(url);
}

void WinRTUtility::closeWebPage() {}

void WinRTUtility::landscapeOrientation() {}

void WinRTUtility::defaultOrientation() {}

void WinRTUtility::showPlayerNotification(bool, QString, QString) {}

void WinRTUtility::hidePlayerNotification() {}

void WinRTUtility::enableKeepScreenOn() {
  QEventDispatcherWinRT::runOnXamlThread([this]() {
    display_request_->RequestActive();
    return true;
  });
}

void WinRTUtility::disableKeepScreenOn() {
  QEventDispatcherWinRT::runOnXamlThread([this]() {
    display_request_->RequestRelease();
    return true;
  });
}

void WinRTUtility::showAd() {}

void WinRTUtility::hideAd() {}

IPlatformUtility::Pointer IPlatformUtility::create() {
  return cloudstorage::util::make_unique<WinRTUtility>();
}

#endif  // WINRT
