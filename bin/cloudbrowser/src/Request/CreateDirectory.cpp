#include "CreateDirectory.h"

#include "CloudContext.h"
#include "CloudItem.h"

using namespace cloudstorage;

void CreateDirectoryRequest::update(CloudContext* context, CloudItem* parent,
                                    const QString& name) {
  set_done(false);
  auto object = new RequestNotifier;
  auto provider = parent->provider().variant();
  connect(this, &Request::errorOccurred, context,
          [=](QVariantMap provider, int code, QString description) {
            emit context->errorOccurred("CreateDirectory", provider, code,
                                        description);
          });
  connect(object, &RequestNotifier::finishedItem, this,
          [=](EitherError<IItem> e) {
            set_done(true);
            emit createdDirectory();
            if (e.left())
              emit errorOccurred(provider, e.left()->code_,
                                 e.left()->description_.c_str());
          });
  auto p = parent->provider().provider_;
  auto r = p->createDirectoryAsync(parent->item(), name.toStdString(),
                                   [object](EitherError<IItem> e) {
                                     emit object->finishedItem(e);
                                     object->deleteLater();
                                   });
  context->add(p, std::move(r));
}
