#ifndef ANDROIDUTILITY_H
#define ANDROIDUTILITY_H

#ifdef __ANDROID__

#include <QAndroidActivityResultReceiver>
#include <QAndroidJniObject>
#include <QObject>

class AndroidUtility : public QObject {
 public:
  AndroidUtility();
  ~AndroidUtility();

  Q_INVOKABLE void openWebPage(QString url);
  Q_INVOKABLE void closeWebPage();
  Q_INVOKABLE void landscapeOrientation();
  Q_INVOKABLE void defaultOrientation();
  Q_INVOKABLE void showPlayerNotification(bool playing, QString filename,
                                          QString title);
  Q_INVOKABLE void hidePlayerNotification();

 signals:
  void notify(QString action);

 private:
  QAndroidJniObject intent_;
  class ResultReceiver : public QAndroidActivityResultReceiver {
   private:
    void handleActivityResult(int receiverRequestCode, int resultCode,
                              const QAndroidJniObject &data) override;
  } receiver_;

  Q_OBJECT
};

#endif  // __ANDROID__
#endif  // ANDROIDUTILITY_H
