#ifndef FUSE_LOW_LEVEL_H
#define FUSE_LOW_LEVEL_H

#ifndef FUSE_USE_VERSION

#ifdef WITH_FUSE
#define FUSE_USE_VERSION 31
#endif

#ifdef WITH_LEGACY_FUSE
#define FUSE_USE_VERSION 26
#endif

#endif

#ifndef _WIN32
#include <fuse_lowlevel.h>
#include <json/json.h>

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

#endif  // _WIN32

#endif  // FUSE_USE_VERSION
#endif  // FUSE_LOW_LEVEL_H
