#include "FuseHighLevel.h"

#include <cstring>
#include <future>
#include <string>
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

int getattr(const char *path, struct stat *stat, struct fuse_file_info *) {
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

int open(const char *path, struct fuse_file_info *) {
  log("opening file", path);
  return 0;
}

int readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t,
            struct fuse_file_info *, enum fuse_readdir_flags) {
  std::promise<int> ret;
  context()->getattr(path, [&](EitherError<IFileSystem::INode> e) {
    if (e.left()) return ret.set_value(-ENOENT);
    context()->readdir(
        e.right()->inode(),
        [&](EitherError<std::vector<IFileSystem::INode::Pointer>> e) {
          if (e.left()) return ret.set_value(-EIO);
          for (auto &&i : *e.right()) {
            filler(buf, context()->sanitize(i->filename()).c_str(), nullptr, 0,
                   fuse_fill_dir_flags());
          }
          ret.set_value(0);
        });
  });
  return ret.get_future().get();
}

int read(const char *path, char *buffer, size_t size, off_t offset,
         struct fuse_file_info *) {
  std::promise<int> ret;
  context()->getattr(path, [&](EitherError<IFileSystem::INode> e) {
    if (e.left()) return ret.set_value(-ENOENT);
    context()->read(e.right()->inode(), offset, size,
                    [&](EitherError<std::string> e) {
                      if (e.left()) return ret.set_value(-EIO);
                      memcpy(buffer, e.right()->data(), e.right()->size());
                      ret.set_value(e.right()->size());
                    });
  });
  return ret.get_future().get();
}

int rename(const char *path, const char *new_path, unsigned int) {
  std::promise<int> ret;
  auto source = file_id(path);
  auto dest = file_id(new_path);
  context()->getattr(source.path_, [&](EitherError<IFileSystem::INode> e1) {
    if (e1.left()) return ret.set_value(-ENOENT);
    context()->getattr(dest.path_, [&](EitherError<IFileSystem::INode> e2) {
      if (e2.left()) return ret.set_value(-ENOENT);
      context()->rename(e1.right()->inode(), source.filename_.c_str(),
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
  context()->getattr(file.path_, [&](EitherError<IFileSystem::INode> e) {
    if (e.left()) return ret.set_value(-ENOENT);
    context()->mkdir(e.right()->inode(), file.filename_.c_str(),
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
  context()->getattr(file.path_, [&](EitherError<IFileSystem::INode> e) {
    if (e.left()) return ret.set_value(-ENOENT);
    context()->remove(e.right()->inode(), file.filename_.c_str(),
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
  context()->getattr(file.path_, [&](EitherError<IFileSystem::INode> e) {
    if (e.left()) {
      log(e.left()->description_);
      return ret.set_value(-ENOENT);
    }
    context()->mknod(e.right()->inode(), file.filename_.c_str());
    ret.set_value(0);
  });
  return ret.get_future().get();
}

int write(const char *path, const char *data, size_t size, off_t offset,
          struct fuse_file_info *) {
  std::promise<int> ret;
  context()->getattr(path, [&](EitherError<IFileSystem::INode> e) {
    if (e.left()) return ret.set_value(-ENOENT);
    context()->write(e.right()->inode(), data, size, offset,
                     [&](EitherError<uint32_t> e) {
                       if (e.left()) return ret.set_value(-ENOENT);
                       ret.set_value(*e.right());
                     });
  });
  return ret.get_future().get();
}

int release(const char *path, struct fuse_file_info *) {
  std::promise<int> ret;
  context()->getattr(path, [&](EitherError<IFileSystem::INode> e) {
    if (e.left()) return ret.set_value(-ENOENT);
    context()->release(e.right()->inode(), [&](EitherError<void> e) {
      if (e.left()) return ret.set_value(-EIO);
      ret.set_value(0);
    });
  });
  return ret.get_future().get();
}

}  // namespace

struct stat item_to_stat(IFileSystem::INode::Pointer i) {
  struct stat ret = {};
  if (i->timestamp() != IItem::UnknownTimeStamp)
    ret.st_mtime = std::chrono::system_clock::to_time_t(i->timestamp());
  if (i->size() != IItem::UnknownSize)
    ret.st_size = i->size();
  else if (i->type() != IItem::FileType::Directory)
    ret.st_size = 1LL << 32;
  ret.st_mode =
      (i->type() == IItem::FileType::Directory ? S_IFDIR : S_IFREG) | 0644;
  ret.st_ino = i->inode();
  return ret;
}

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
  operations.release = release;
  return operations;
}

}  // namespace cloudstorage
