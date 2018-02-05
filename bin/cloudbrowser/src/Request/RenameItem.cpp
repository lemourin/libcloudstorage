#include "RenameItem.h"

#include "CloudContext.h"
#include "CloudItem.h"

using namespace cloudstorage;

void RenameItemRequest::update(CloudContext* context, CloudItem* item,
                               QString name) {
  set_done(false);
  auto object = new RequestNotifier;
  auto provider = item->provider().variant();
  connect(this, &Request::errorOccurred, context,
          [=](QVariantMap provider, int code, QString description) {
            emit context->errorOccurred("RenameItem", provider, code,
                                        description);
          });
  connect(object, &RequestNotifier::finishedItem, this,
          [=](EitherError<IItem> e) {
            set_done(true);
            emit itemRenamed();
            if (e.left())
              emit errorOccurred(provider, e.left()->code_,
                                 e.left()->description_.c_str());
          });
  auto p = item->provider().provider_;
  auto r = p->renameItemAsync(item->item(), name.toStdString(),
                              [object](EitherError<IItem> e) {
                                emit object->finishedItem(e);
                                object->deleteLater();
                              });
  context->add(p, std::move(r));
}
