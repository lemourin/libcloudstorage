#ifndef IOSUTILITY_H
#define IOSUTILITY_H

#include "IPlatformUtility.h"

#ifdef IOS

class IOSUtility : public IPlatformUtility {
 public:
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
};

#endif

#endif  // IOSUTILITY_H
