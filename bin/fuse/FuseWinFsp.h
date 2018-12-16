#ifndef FUSE_WINFSP
#define FUSE_WINFSP

#ifdef WITH_WINFSP

#include <winfsp/winfsp.h>

struct fuse_args;

namespace cloudstorage {

class IFileSystem;

struct FuseWinFsp {
  FuseWinFsp(fuse_args *args, const char *mountpoint, void *userdata);
  ~FuseWinFsp();

  int run(bool singlethread, bool) const;

  IFileSystem **fs_;
};

FSP_FILE_SYSTEM_INTERFACE winfsp_operations();

}  // namespace cloudstorage

#endif  // WITH_WINFSP
#endif  // FUSE_WINFSP