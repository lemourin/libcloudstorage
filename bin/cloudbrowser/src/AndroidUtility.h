#ifndef ANDROIDUTILITY_H
#define ANDROIDUTILITY_H

#ifdef __ANDROID__

#include <QAndroidJniObject>
#include <QObject>
#include <QtAndroid>

#include "Exec.h"
#include "IPlatformUtility.h"
#include "IRequest.h"

class CLOUDBROWSER_API AndroidUtility : public IPlatformUtility {
 public:
  class IResultListener
      : public cloudstorage::IGenericCallback<int, int,
                                              const QAndroidJniObject&> {
   public:
    ~IResultListener() override;

   private:
    friend class AndroidUtility;
    mutable int code_;
  };
  AndroidUtility();
  ~AndroidUtility();

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

  static QAndroidJniObject activity();

  static void startActivity(const QAndroidJniObject& intent, int request_code,
                            const IResultListener* = nullptr);

 private:
  QAndroidJniObject intent_;
};

#endif  // __ANDROID__
#endif  // ANDROIDUTILITY_H
