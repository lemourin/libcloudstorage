#include "CloudRequest.h"

void Request::set_done(bool done) {
  if (done_ == done) return;
  done_ = done;
  emit doneChanged();
}
