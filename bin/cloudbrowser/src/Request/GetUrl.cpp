#include "GetUrl.h"

#include "CloudContext.h"
#include "CloudItem.h"

using namespace cloudstorage;

void GetUrlRequest::update(CloudContext* context, CloudItem* item) {
  set_done(false);
  auto object = new RequestNotifier;
  auto provider = item->provider().variant();
  connect(this, &Request::errorOccurred, context,
          [=](QVariantMap provider, int code, QString description) {
            emit context->errorOccurred("GetItemUrl", provider, code,
                                        description);
          });
  connect(object, &RequestNotifier::finishedString, this,
          [=](EitherError<std::string> e) {
            if (e.left()) {
              source_ = "";
              emit errorOccurred(provider, e.left()->code_,
                                 e.left()->description_.c_str());
            } else
              source_ = e.right()->c_str();
            emit sourceChanged();
            set_done(true);
          });
  auto p = item->provider().provider_;
  auto r = p->getFileDaemonUrlAsync(item->item(),
                                    [object](EitherError<std::string> e) {
                                      emit object->finishedString(e);
                                      object->deleteLater();
                                    });
  context->add(p, std::move(r));
}
