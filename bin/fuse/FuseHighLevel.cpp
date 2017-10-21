#include "FuseHighLevel.h"

#include <cstring>
#include <future>
#include <string>
#include "FuseCommon.h"
#include "IFileSystem.h"
#include "Utility/CurlHttp.h"
#include "Utility/Utility.h"

namespace cloudstorage {

using util::log;

namespace {

struct FileId {
  std::string path_;
  std::string filename_;
};

FileId file_id(std::string path) {
  if (path == "/") return {"/", ""};
  if (path.back() == '/') path.pop_back();
  auto it = path.find_last_of('/');
  return {path.substr(0, it), path.substr(it + 1)};
}

IFileSystem *context() {
  return *static_cast<IFileSystem **>(fuse_get_context()->private_data);
}

int getattr(const char *path, struct FUSE_STAT *stat) {
  std::promise<int> ret;
  context()->getattr(path, [&](EitherError<IFileSystem::INode> e) {
    if (e.left()) {
      log("getattr:", path, e.left()->description_);
      stat->st_mode = S_IFREG | 0644;
      return ret.set_value(-ENOENT);
    }
    *stat = item_to_stat(e.right());
    ret.set_value(0);
  });
  return ret.get_future().get();
}

int opendir(const char *, struct fuse_file_info *) { return 0; }

int open(const char *, struct fuse_file_info *f) { return 0; }

int readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t,
            struct fuse_file_info *) {
  std::promise<int> ret;
  auto ctx = context();
  ctx->getattr(path, [&](EitherError<IFileSystem::INode> e) {
    if (e.left()) return ret.set_value(-ENOENT);
    ctx->readdir(e.right()->inode(),
                 [&](EitherError<std::vector<IFileSystem::INode::Pointer>> e) {
                   if (e.left()) return ret.set_value(-EIO);
                   for (auto &&i : *e.right())
                     filler(buf, ctx->sanitize(i->filename()).c_str(), nullptr,
                            0);
                   ret.set_value(0);
                 });
  });
  return ret.get_future().get();
}

int read(const char *path, char *buffer, size_t size, off_t offset,
         struct fuse_file_info *) {
  std::promise<int> ret;
  auto ctx = context();
  ctx->getattr(path, [&](EitherError<IFileSystem::INode> e) {
    if (e.left()) return ret.set_value(-ENOENT);
    ctx->read(e.right()->inode(), offset, size,
              [&](EitherError<std::string> e) {
                if (e.left()) return ret.set_value(-EIO);
                memcpy(buffer, e.right()->data(), e.right()->size());
                ret.set_value(e.right()->size());
              });
  });
  return ret.get_future().get();
}

int rename(const char *path, const char *new_path) {
  std::promise<int> ret;
  auto source = file_id(path);
  auto dest = file_id(new_path);
  auto ctx = context();
  ctx->getattr(source.path_, [&](EitherError<IFileSystem::INode> e1) {
    if (e1.left()) return ret.set_value(-ENOENT);
    ctx->getattr(dest.path_, [&](EitherError<IFileSystem::INode> e2) {
      if (e2.left()) return ret.set_value(-ENOENT);
      ctx->rename(e1.right()->inode(), source.filename_.c_str(),
                  e2.right()->inode(), dest.filename_.c_str(),
                  [&](EitherError<IItem> e) {
                    if (e.left()) {
                      log("rename: ", e.left()->description_);
                      return ret.set_value(-EIO);
                    }
                    ret.set_value(0);
                  });
    });
  });
  return ret.get_future().get();
}

int mkdir(const char *path, mode_t) {
  std::promise<int> ret;
  auto file = file_id(path);
  auto ctx = context();
  ctx->getattr(file.path_, [&](EitherError<IFileSystem::INode> e) {
    if (e.left()) return ret.set_value(-ENOENT);
    ctx->mkdir(e.right()->inode(), file.filename_.c_str(),
               [&](EitherError<IFileSystem::INode> e) {
                 if (e.left()) return ret.set_value(-EIO);
                 ret.set_value(0);
               });
  });
  return ret.get_future().get();
}

int remove(const char *path) {
  std::promise<int> ret;
  auto file = file_id(path);
  auto ctx = context();
  ctx->getattr(file.path_, [&](EitherError<IFileSystem::INode> e) {
    if (e.left()) return ret.set_value(-ENOENT);
    ctx->remove(e.right()->inode(), file.filename_.c_str(),
                [&](EitherError<void> e) {
                  if (e.left()) {
                    if (e.left()->code_ == IFileSystem::NotEmpty)
                      return ret.set_value(-ENOTEMPTY);
                    else
                      return ret.set_value(-ENOENT);
                  }
                  ret.set_value(0);
                });
  });
  return ret.get_future().get();
}

int mknod(const char *path, mode_t, dev_t) {
  std::promise<int> ret;
  auto file = file_id(path);
  auto ctx = context();
  ctx->getattr(file.path_, [&](EitherError<IFileSystem::INode> e) {
    if (e.left()) {
      log(e.left()->description_);
      return ret.set_value(-ENOENT);
    }
    ctx->mknod(e.right()->inode(), file.filename_.c_str());
    ret.set_value(0);
  });
  return ret.get_future().get();
}

int write(const char *path, const char *data, size_t size, off_t offset,
          struct fuse_file_info *) {
  std::promise<int> ret;
  auto ctx = context();
  ctx->getattr(path, [&](EitherError<IFileSystem::INode> e) {
    if (e.left()) return ret.set_value(-ENOENT);
    ctx->write(e.right()->inode(), data, size, offset,
               [&](EitherError<uint32_t> e) {
                 if (e.left()) return ret.set_value(-ENOENT);
                 ret.set_value(*e.right());
               });
  });
  return ret.get_future().get();
}

int fsync(const char *path, int, struct fuse_file_info *) {
  std::promise<int> ret;
  auto ctx = context();
  ctx->getattr(path, [&](EitherError<IFileSystem::INode> e) {
    if (e.left()) return ret.set_value(-ENOENT);
    ctx->fsync(e.right()->inode(), [&](EitherError<void> e) {
      if (e.left()) return ret.set_value(-EIO);
      ret.set_value(0);
    });
  });
  return ret.get_future().get();
}

int flush(const char *path, struct fuse_file_info *fi) {
  return fsync(path, 0, fi);
}

}  // namespace

fuse_operations high_level_operations() {
  fuse_operations operations = {};
  operations.getattr = getattr;
  operations.opendir = opendir;
  operations.readdir = readdir;
  operations.read = read;
  operations.open = open;
  operations.rename = rename;
  operations.mkdir = mkdir;
  operations.rmdir = remove;
  operations.unlink = remove;
  operations.write = write;
  operations.mknod = mknod;
  operations.fsync = fsync;
  return operations;
}

int fuse_parse_cmdline(struct fuse_args *args, fuse_cmdline_opts *opts) {
  int ret = fuse_parse_cmdline(args, &opts->mountpoint, &opts->singlethread,
                               &opts->foreground);
  opts->singlethread ^= 1;
  return ret;
}

void fuse_cmdline_help() {}

FuseHighLevel::FuseHighLevel(fuse_args *args, const char *mountpoint,
                             void *userdata)
    : mountpoint_(mountpoint) {
  auto operations = cloudstorage::high_level_operations();
  channel_ = fuse_mount(mountpoint, args);
  fuse_ = fuse_new(channel_, args, &operations, sizeof(operations), userdata);
  session_ = fuse_get_session(fuse_);
  fuse_set_signal_handlers(session_);
}

FuseHighLevel::~FuseHighLevel() {
  fuse_remove_signal_handlers(session_);
  fuse_unmount(mountpoint_.c_str(), channel_);
  fuse_destroy(fuse_);
}

int FuseHighLevel::run(bool singlethread, bool) const {
  return singlethread ? fuse_loop(fuse_) : fuse_loop_mt(fuse_);
}

}  // namespace cloudstorage
