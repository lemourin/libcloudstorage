#ifndef ANDROIDUTILITY_H
#define ANDROIDUTILITY_H

#ifdef __ANDROID__

#include <QAndroidActivityResultReceiver>
#include <QAndroidJniObject>
#include <QObject>
#include "IPlatformUtility.h"

class AndroidUtility : public IPlatformUtility {
 public:
  AndroidUtility();
  ~AndroidUtility();

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
  QAndroidJniObject intent_;
  class ResultReceiver : public QAndroidActivityResultReceiver {
   private:
    void handleActivityResult(int receiverRequestCode, int resultCode,
                              const QAndroidJniObject &data) override;
  } receiver_;
};

#endif  // __ANDROID__
#endif  // ANDROIDUTILITY_H
