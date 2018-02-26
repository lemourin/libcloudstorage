#ifndef IFILE_SYSTEM_H
#define IFILE_SYSTEM_H

#include <chrono>
#include <cstdint>
#include <memory>
#include "ICloudProvider.h"
#include "IItem.h"
#include "IRequest.h"

namespace cloudstorage {

class IFileSystem {
 public:
  class INode;

  using FileId = uint64_t;
  using Pointer = std::unique_ptr<IFileSystem>;

  static constexpr int NotEmpty = 1001;

  class INode {
   public:
    using Pointer = std::shared_ptr<INode>;
    using List = std::vector<std::shared_ptr<INode>>;

    virtual ~INode() = default;

    virtual FileId inode() const = 0;
    virtual std::chrono::system_clock::time_point timestamp() const = 0;
    virtual uint64_t size() const = 0;
    virtual std::string filename() const = 0;
    virtual IItem::FileType type() const = 0;
  };

  using ListDirectoryCallback = GenericCallback<EitherError<INode::List>>;
  using GetItemCallback = GenericCallback<EitherError<INode>>;
  using DownloadItemCallback = GenericCallback<EitherError<std::string>>;
  using WriteDataCallback = GenericCallback<EitherError<uint32_t>>;
  using DataSynchronizedCallback = GenericCallback<EitherError<void>>;

  struct ProviderEntry {
    std::string label_;
    std::shared_ptr<ICloudProvider> provider_;
  };

  virtual ~IFileSystem() = default;

  static IFileSystem::Pointer create(const std::vector<ProviderEntry> &,
                                     IHttp::Pointer http,
                                     const std::string &temporary_directory);

  virtual std::string sanitize(const std::string &filename) = 0;

  virtual FileId mknod(FileId parent, const char *name) = 0;

  virtual void lookup(FileId parent_node, const std::string &name,
                      GetItemCallback) = 0;

  virtual void getattr(FileId node, GetItemCallback) = 0;

  virtual void getattr(const std::string &path, GetItemCallback) = 0;

  virtual void write(FileId node, const char *data, uint32_t size,
                     uint64_t offset, WriteDataCallback) = 0;

  virtual void readdir(FileId node, ListDirectoryCallback) = 0;

  virtual void read(FileId node, size_t offset, size_t size,
                    DownloadItemCallback) = 0;

  virtual void rename(FileId parent, const char *name, FileId newparent,
                      const char *newname, RenameItemCallback) = 0;

  virtual void mkdir(FileId parent, const char *name, GetItemCallback) = 0;

  virtual void remove(FileId parent, const char *name, DeleteItemCallback) = 0;

  virtual void fsync(FileId, DataSynchronizedCallback) = 0;
};

}  // namespace cloudstorage

#endif  // IFILE_SYSTEM_H
