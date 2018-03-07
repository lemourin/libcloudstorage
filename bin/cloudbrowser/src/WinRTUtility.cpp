#include "WinRTUtility.h"

#ifdef WINRT

#include <private/qeventdispatcher_winrt_p.h>
#include <qfunctions_winrt.h>
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
using namespace Microsoft::Advertising::WinRT::UI;

WinRTUtility::AdEventHandler::AdEventHandler(WinRTUtility* winrt)
    : winrt_(winrt) {}

void WinRTUtility::AdEventHandler::onAdError(Object ^, AdErrorEventArgs ^) {
  emit winrt_->notify("HIDE_AD");
}

void WinRTUtility::AdEventHandler::onAdRefreshed(Object ^, RoutedEventArgs ^) {
  emit winrt_->notify("SHOW_AD");
}

WinRTUtility::WinRTUtility() : ad_control_attached_() {
  QEventDispatcherWinRT::runOnXamlThread([this]() {
    ad_event_handler_ = ref new AdEventHandler(this);
    ad_control_ = ref new AdControl();
    ad_control_->ApplicationId = L"9nbkjrh757k7";
    ad_control_->AdUnitId = L"1100015829";
    ad_control_->Width = 320;
    ad_control_->Height = 50;
    ad_control_->VerticalAlignment = VerticalAlignment::Bottom;
    ad_control_->AdRefreshed += ref new EventHandler<RoutedEventArgs ^>(
        ad_event_handler_, &AdEventHandler::onAdRefreshed);
    ad_control_->ErrorOccurred += ref new EventHandler<AdErrorEventArgs ^>(
        ad_event_handler_, &AdEventHandler::onAdError);

    display_request_ = ref new DisplayRequest;

    return true;
  });
}

WinRTUtility::~WinRTUtility() {}

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

void WinRTUtility::showAd() {
  QEventDispatcherWinRT::runOnXamlThread([this]() {
    if (!ad_control_attached_ && qGuiApp->focusWindow()) {
      ad_control_attached_ = true;
      ComPtr<IInspectable> root_inspect =
          reinterpret_cast<IInspectable*>(qGuiApp->focusWindow()->winId());
      SwapChainPanel ^ root =
          reinterpret_cast<SwapChainPanel ^>(root_inspect.Get());
      root->Children->Append(ad_control_);
    }
    ad_control_->Visibility = Visibility::Visible;
    return true;
  });
}

void WinRTUtility::hideAd() {
  QEventDispatcherWinRT::runOnXamlThread([this]() {
    ad_control_->Visibility = Visibility::Collapsed;
    return true;
  });
}

IPlatformUtility::Pointer IPlatformUtility::create() {
  return cloudstorage::util::make_unique<WinRTUtility>();
}

#endif  // WINRT
