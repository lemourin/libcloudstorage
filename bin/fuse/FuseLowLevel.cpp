#include "FuseLowLevel.h"

#include <cassert>
#include <cfloat>
#include <cstring>
#include <iostream>
#include <sstream>

#include "FuseCommon.h"
#include "IFileSystem.h"
#include "Utility/CurlHttp.h"
#include "Utility/Utility.h"

namespace cloudstorage {

using util::log;

namespace {

IFileSystem *context(fuse_req_t req) {
  return *static_cast<IFileSystem **>(fuse_req_userdata(req));
}

void getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *) {
  context(req)->getattr(ino, [=](EitherError<IFileSystem::INode> e) {
    if (auto i = e.right()) {
      auto stat = item_to_stat(i);
      fuse_reply_attr(req, &stat, 1);
    } else {
      log("getattr:", e.left()->code_, e.left()->description_);
      fuse_reply_err(req, ENOENT);
    }
  });
}

void opendir(fuse_req_t req, fuse_ino_t, struct fuse_file_info *fi) {
  fuse_reply_open(req, fi);
}

void open(fuse_req_t req, fuse_ino_t, struct fuse_file_info *fi) {
  fi->keep_cache = true;
  fuse_reply_open(req, fi);
}

void release(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *) {
  context(req)->release(ino, [=](EitherError<void> e) {
    if (e.left()) {
      log("release:", e.left()->code_, e.left()->description_);
      fuse_reply_err(req, EINVAL);
    } else
      fuse_reply_err(req, 0);
  });
}

void read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off,
          struct fuse_file_info *) {
  context(req)->read(ino, off, size, [=](EitherError<std::string> e) {
    if (auto data = e.right()) {
      fuse_reply_buf(req, data->data(), data->size());
    } else {
      log("read:", e.left()->code_, e.left()->description_);
      fuse_reply_err(req, ENOENT);
    }
  });
}

void readdir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off,
             struct fuse_file_info *) {
  context(req)->readdir(
      ino, [=](EitherError<std::vector<IFileSystem::INode::Pointer>> e) {
        if (auto lst = e.right()) {
          std::string data;
          for (size_t i = off; i < lst->size(); i++) {
            auto item = lst->at(i);
            auto name = context(req)->sanitize(item->filename());
            auto sz =
                fuse_add_direntry(req, nullptr, 0, name.c_str(), nullptr, 0);
            if (data.size() + sz > size) {
              fuse_reply_buf(req, data.data(), data.size());
              return;
            }
            std::vector<char> buffer(sz);
            auto stat = item_to_stat(lst->at(i));
            fuse_add_direntry(req, buffer.data(), buffer.size(), name.c_str(),
                              &stat, i + 1);
            data += std::string(buffer.begin(), buffer.end());
          }
          fuse_reply_buf(req, data.data(), data.size());
        } else {
          log("readdir:", e.left()->code_, e.left()->description_);
          fuse_reply_err(req, ENOENT);
        }
      });
}

void lookup(fuse_req_t req, fuse_ino_t parent, const char *name) {
  context(req)->lookup(parent, name, [=](EitherError<IFileSystem::INode> e) {
    if (auto node = e.right()) {
      fuse_entry_param entry = {};
      entry.ino = node->inode();
      entry.attr = item_to_stat(node);
      entry.attr_timeout = 1;
      entry.entry_timeout = 1;
      entry.generation = 1;
      fuse_reply_entry(req, &entry);
    } else {
      log("lookup:", name, e.left()->code_, e.left()->description_);
      fuse_reply_err(req, ENOENT);
    }
  });
}

void rename(fuse_req_t req, fuse_ino_t parent, const char *name,
            fuse_ino_t newparent, const char *newname, unsigned int) {
  context(req)->rename(
      parent, name, newparent, newname, [=](EitherError<IItem> e) {
        if (e.left()) {
          log("rename:", e.left()->code_, e.left()->description_);
          fuse_reply_err(req, ENOSYS);
        } else
          fuse_reply_err(req, 0);
      });
}

void mkdir(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t) {
  context(req)->mkdir(parent, name, [=](EitherError<IFileSystem::INode> e) {
    if (e.left()) {
      log("mkdir:", e.left()->code_, e.left()->description_);
      fuse_reply_err(req, ENOSYS);
    } else {
      auto node = e.right();
      fuse_entry_param entry = {};
      entry.ino = node->inode();
      entry.attr = item_to_stat(node);
      entry.attr_timeout = 1;
      entry.entry_timeout = 1;
      entry.generation = 1;
      fuse_reply_entry(req, &entry);
    }
  });
}

void remove(fuse_req_t req, fuse_ino_t parent, const char *name) {
  context(req)->remove(parent, name, [=](EitherError<void> e) {
    if (e.left()) {
      if (e.left()->code_ == IFileSystem::NotEmpty)
        fuse_reply_err(req, ENOTEMPTY);
      else {
        log("remove:", e.left()->code_, e.left()->description_);
        fuse_reply_err(req, ENOSYS);
      }
    } else {
      fuse_reply_err(req, 0);
    }
  });
}

void mknod(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t, dev_t) {
  auto inode = context(req)->mknod(parent, name);
  fuse_entry_param entry = {};
  entry.ino = inode;
  entry.attr.st_mode = S_IFREG | 0644;
  entry.attr_timeout = DBL_MAX;
  entry.entry_timeout = DBL_MAX;
  entry.generation = 1;
  fuse_reply_entry(req, &entry);
}

void write(fuse_req_t req, fuse_ino_t ino, const char *data, size_t size,
           off_t off, struct fuse_file_info *) {
  context(req)->write(ino, data, size, off, [=](EitherError<uint32_t> e) {
    if (e.left()) {
      log("write:", e.left()->code_, e.left()->description_);
      fuse_reply_err(req, ENOSYS);
    } else {
      fuse_reply_write(req, *e.right());
    }
  });
}

}  // namespace

fuse_lowlevel_ops low_level_operations() {
  fuse_lowlevel_ops operations = {};
  operations.getattr = getattr;
  operations.opendir = opendir;
  operations.readdir = readdir;
  operations.lookup = lookup;
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

FuseLowLevel::FuseLowLevel(fuse_args *args, const char *mountpoint,
                           void *userdata) {
  auto operations = cloudstorage::low_level_operations();
  session_ = fuse_session_new(args, &operations, sizeof(operations), userdata);
  fuse_set_signal_handlers(session_);
  fuse_session_mount(session_, mountpoint);
}

FuseLowLevel::~FuseLowLevel() {
  fuse_session_unmount(session_);
  fuse_remove_signal_handlers(session_);
  fuse_session_destroy(session_);
}

int FuseLowLevel::run(bool singlethread, bool clone_fd) const {
  return singlethread ? fuse_session_loop(session_)
                      : fuse_session_loop_mt(session_, clone_fd);
}

}  // namespace cloudstorage
