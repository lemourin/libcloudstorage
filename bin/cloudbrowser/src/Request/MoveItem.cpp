#include "MoveItem.h"

#include "CloudContext.h"
#include "CloudItem.h"

using namespace cloudstorage;

void MoveItemRequest::update(CloudContext* context, CloudItem* source,
                             CloudItem* destination) {
  set_done(false);
  auto object = new RequestNotifier;
  auto provider = source->provider().variant();
  connect(this, &Request::errorOccurred, context,
          [=](QVariantMap provider, int code, QString description) {
            emit context->errorOccurred("MoveItem", provider, code,
                                        description);
          });
  connect(object, &RequestNotifier::finishedItem, this,
          [=](EitherError<IItem> e) {
            set_done(true);
            emit itemMoved();
            if (e.left())
              emit errorOccurred(provider, e.left()->code_,
                                 e.left()->description_.c_str());
          });
  auto p = source->provider().provider_;
  auto r = p->moveItemAsync(source->item(), destination->item(),
                            [object](EitherError<IItem> e) {
                              emit object->finishedItem(e);
                              object->deleteLater();
                            });
  context->add(p, std::move(r));
}
