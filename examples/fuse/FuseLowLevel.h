#ifndef FUSE_LOW_LEVEL_H
#define FUSE_LOW_LEVEL_H

#define FUSE_USE_VERSION 31
#include <fuse_lowlevel.h>

namespace cloudstorage {
fuse_lowlevel_ops low_level_operations();
}  // namespace cloudstorage

#endif  // FUSE_LOW_LEVEL_H
