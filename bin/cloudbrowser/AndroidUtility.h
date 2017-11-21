#ifndef ANDROIDUTILITY_H
#define ANDROIDUTILITY_H

#include <QAndroidActivityResultReceiver>
#include <QAndroidJniObject>
#include <QObject>

class AndroidUtility : public QObject {
 public:
  Q_INVOKABLE void openWebPage(QString url);
  Q_INVOKABLE void closeWebPage();
  Q_INVOKABLE void landscapeOrientation();
  Q_INVOKABLE void defaultOrientation();

 private:
  QAndroidJniObject intent_;
  class ResultReceiver : public QAndroidActivityResultReceiver {
   private:
    void handleActivityResult(int receiverRequestCode, int resultCode,
                              const QAndroidJniObject &data) override;
  } receiver_;

  Q_OBJECT
};

#endif  // ANDROIDUTILITY_H
