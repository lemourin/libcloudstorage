#ifndef FUSE_LOW_LEVEL_H
#define FUSE_LOW_LEVEL_H

#ifdef WITH_FUSE

#define FUSE_USE_VERSION 31
#include <fuse_lowlevel.h>

#include <json/json.h>

namespace cloudstorage {

struct FuseLowLevel {
  FuseLowLevel(fuse_args *args, const char *mountpoint, void *userdata);
  ~FuseLowLevel();

  int run(bool singlethread, bool clone_fd) const;

  fuse_session *session_;
};

fuse_lowlevel_ops low_level_operations();

}  // namespace cloudstorage

#endif  // WITH_FUSE
#endif  // FUSE_LOW_LEVEL_H
