#ifndef DESKTOPUTILITY_H
#define DESKTOPUTILITY_H

#include "IPlatformUtility.h"
#include "WinRTUtility.h"

#ifndef __ANDROID__
#ifndef WINRT
#ifndef IOS

#define USE_DESKTOP_UTILITY

#include <condition_variable>
#include <mutex>
#include <thread>

#include "Exec.h"

class DesktopUtility : public IPlatformUtility {
 public:
  DesktopUtility();
  ~DesktopUtility() override;

  void initialize(QWindow*) const override;
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
  std::mutex mutex_;
  std::condition_variable condition_;
  std::atomic_bool screensaver_enabled_;
  bool running_ = true;
  std::thread thread_;
};

#endif  // IOS
#endif  // WINRT
#endif  // __ANDROID__

#endif  // DESKTOPUTILITY_H
