#ifndef FUSE_DOKAN_H
#define FUSE_DOKAN_H

#ifdef WITH_DOKAN

#include <dokan.h>
#include <string>

struct fuse_args;

namespace cloudstorage {

const auto BUFFER_SIZE = 1024;

struct FuseDokan {
  FuseDokan(fuse_args *args, const char *mountpoint, void *userdata);
  ~FuseDokan();

  int run(bool singlethread, bool) const;

  std::wstring mountpoint_;
  DOKAN_OPTIONS options_;
};

DOKAN_OPERATIONS dokan_operations();

}  // namespace cloudstorage

#endif

#endif  // FUSE_DOKAN_H