#ifndef WINRT_UTILITY
#define WINRT_UTILITY

#ifdef _WIN32
#include <windows.h>

#if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
#define WINRT
#endif

#endif

#ifdef WINRT

#include "IPlatformUtility.h"

class WinRTUtility : public IPlatformUtility {
 public:
  WinRTUtility();
  ~WinRTUtility();

  bool mobile() const override;
  QString name() const override;
  bool openWebPage(QString url) override;
  void closeWebPage() override;
  void landscapeOrientation() override;
  void defaultOrientation() override;
  void showPlayerNotification(bool playing, QString filename,
                              QString title) override;
  void hidePlayerNotification() override;
  void enableKeepScreenOn() override;
  void disableKeepScreenOn() override;
  void showAd() override;
  void hideAd() override;

 private:
  // clang-format off
  ref class AdEventHandler sealed {
   internal:
    AdEventHandler(WinRTUtility*);
    void onAdRefreshed(Platform::Object ^,
                       Windows::UI::Xaml::RoutedEventArgs ^);
    void onAdError(Platform::Object ^,
                   Microsoft::Advertising::WinRT::UI::AdErrorEventArgs ^);

   private:
    WinRTUtility* winrt_;
  };
  // clang-format on

  Microsoft::Advertising::WinRT::UI::AdControl ^ ad_control_;
  bool ad_control_attached_;
  AdEventHandler ^ ad_event_handler_;
  Windows::System::Display::DisplayRequest ^ display_request_;
};

#endif  // WINRT
#endif  // WINRT_UTILITY
