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

  void initialize(QWindow* window) const override;
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
  Windows::System::Display::DisplayRequest ^ display_request_;
};

#endif  // WINRT
#endif  // WINRT_UTILITY
