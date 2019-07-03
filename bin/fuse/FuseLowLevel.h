#ifndef FUSE_LOW_LEVEL_H
#define FUSE_LOW_LEVEL_H

#ifdef WITH_FUSE
#include <json/json.h>
#include "FuseCommon.h"

#define FUSE_LOWLEVEL

#ifdef FUSE_USE_VERSION

namespace cloudstorage {

struct FuseLowLevel {
  FuseLowLevel(fuse_args *args, const char *mountpoint, void *userdata);
  ~FuseLowLevel();

  int run(bool singlethread, bool clone_fd) const;

  fuse_session *session_;
#ifdef WITH_LEGACY_FUSE
  fuse_chan *channel_;
#endif
  std::string mountpoint_;
};

fuse_lowlevel_ops low_level_operations();

}  // namespace cloudstorage

#endif  // WITH_FUSE

#endif  // FUSE_USE_VERSION
#endif  // FUSE_LOW_LEVEL_H
