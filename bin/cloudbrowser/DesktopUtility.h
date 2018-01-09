#ifndef DESKTOPUTILITY_H
#define DESKTOPUTILITY_H

#ifndef __ANDROID__

#include "IPlatformUtility.h"

class DesktopUtility : public IPlatformUtility {
 public:
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
};

#endif  // __ANDROID__

#endif  // DESKTOPUTILITY_H
