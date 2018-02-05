#include "DeleteItem.h"

#include "CloudContext.h"
#include "CloudItem.h"

using namespace cloudstorage;

void DeleteItemRequest::update(CloudContext* context, CloudItem* item) {
  set_done(false);
  auto object = new RequestNotifier;
  auto provider = item->provider().variant();
  connect(this, &Request::errorOccurred, context,
          [=](QVariantMap provider, int code, QString description) {
            emit context->errorOccurred("DeleteItem", provider, code,
                                        description);
          });
  connect(object, &RequestNotifier::finishedVoid, this,
          [=](EitherError<void> e) {
            set_done(true);
            emit itemDeleted();
            if (e.left())
              emit errorOccurred(provider, e.left()->code_,
                                 e.left()->description_.c_str());
          });
  auto p = item->provider().provider_;
  auto r = p->deleteItemAsync(item->item(), [object](EitherError<void> e) {
    emit object->finishedVoid(e);
    object->deleteLater();
  });
  context->add(p, std::move(r));
}
