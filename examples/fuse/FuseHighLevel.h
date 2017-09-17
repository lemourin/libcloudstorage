#ifndef FUSE_HIGH_LEVEL_H
#define FUSE_HIGH_LEVEL_H

#define FUSE_USE_VERSION 31
#include <fuse.h>

#include "IFileSystem.h"

namespace cloudstorage {

struct stat item_to_stat(IFileSystem::INode::Pointer i);
fuse_operations high_level_operations();

}  // namespace cloudstorage

#endif  // FUSE_HIGH_LEVEL_H
