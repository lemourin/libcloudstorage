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
  Microsoft::Advertising::WinRT::UI::AdControl ^ ad_control_;
  bool ad_control_attached_;
};

#endif  // WINRT
#endif  // WINRT_UTILITY
