#ifndef FILE_SYSTEM_H
#define FILE_SYSTEM_H

#include <atomic>
#include <condition_variable>
#include <deque>
#include <fstream>
#include <future>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

#include "ICloudStorage.h"
#include "IFileSystem.h"
#include "Utility/Utility.h"

namespace cloudstorage {

const int READ_AHEAD = 2 * 1024 * 1024;
const int CACHED_CHUNK_COUNT = 4;

class FileSystem : public IFileSystem {
 public:
  using mutex = std::recursive_mutex;

  class Node : public IFileSystem::INode {
   public:
    using Pointer = std::shared_ptr<Node>;

    Node();
    Node(std::shared_ptr<ICloudProvider> p, IItem::Pointer item, FileId parent,
         FileId inode, uint64_t size);
    ~Node();

    FileId inode() const override;
    std::chrono::system_clock::time_point timestamp() const override;
    uint64_t size() const override;
    std::string filename() const override;
    IItem::FileType type() const override;

    IItem::Pointer item() const;
    std::shared_ptr<ICloudProvider> provider() const;
    std::shared_ptr<IGenericRequest> upload_request() const;
    void set_upload_request(std::shared_ptr<IGenericRequest> r);
    void set_size(uint64_t size);

   private:
    friend class FileSystem;

    struct Chunk {
      Range range_;
      std::string data_;
    };

    struct ReadRequest {
      Range range_;
      DownloadItemCallback callback_;

      bool operator==(const ReadRequest &r) const { return range_ == r.range_; }
    };

    mutex mutex_;
    std::shared_ptr<ICloudProvider> provider_;
    IItem::Pointer item_;
    FileId parent_;
    FileId inode_;
    uint64_t size_;
    std::shared_ptr<IGenericRequest> upload_request_;
    std::vector<ReadRequest> read_request_;
    std::vector<Range> pending_download_;
    std::deque<Chunk> chunk_;
    std::string cache_filename_;
    std::unique_ptr<std::fstream> store_;
  };

  FileSystem(const std::vector<ProviderEntry> &, IHttp::Pointer http,
             const std::string &temporary_directory);
  ~FileSystem();

  FileId mknod(FileId parent, const char *name) override;
  void lookup(FileId parent_node, const std::string &name,
              GetItemCallback) override;
  void getattr(FileId node, GetItemCallback) override;
  void getattr(const std::string &path, GetItemCallback) override;
  void write(FileId node, const char *data, uint32_t size, uint64_t offset,
             WriteDataCallback) override;
  void readdir(FileId node, ListDirectoryCallback) override;
  void read(FileId node, size_t offset, size_t size,
            DownloadItemCallback) override;
  void rename(FileId parent, const char *name, FileId newparent,
              const char *newname, RenameItemCallback) override;
  void mkdir(FileId parent, const char *name, GetItemCallback) override;
  void remove(FileId parent, const char *name, DeleteItemCallback) override;
  void fsync(FileId, DataSynchronizedCallback) override;
  std::string sanitize(const std::string &) override;

 private:
  struct RequestData {
    std::shared_ptr<ICloudProvider> provider_;
    std::shared_ptr<IGenericRequest> request_;
  };

  void add(RequestData r);
  Node::Pointer add(std::shared_ptr<ICloudProvider>, FileId parent,
                    IItem::Pointer);

  void set(FileId, Node::Pointer);

  Node::Pointer get(FileId node);
  void get_path(FileId node, const std::string &path, GetItemCallback);

  void invalidate(FileId);
  void cleanup();
  void cancelled();
  void cancel(std::shared_ptr<IGenericRequest>);

  void list_directory_async(std::shared_ptr<ICloudProvider>, IItem::Pointer,
                            cloudstorage::ListDirectoryCallback);
  void download_item_async(std::shared_ptr<ICloudProvider>, IItem::Pointer,
                           Range, DownloadItemCallback);
  void get_url_async(std::shared_ptr<ICloudProvider>, IItem::Pointer,
                     GetItemUrlCallback);
  void rename_async(std::shared_ptr<ICloudProvider>, IItem::Pointer item,
                    IItem::Pointer parent, IItem::Pointer destination,
                    const char *name, RenameItemCallback);

  mutable mutex node_data_mutex_;
  mutable mutex request_data_mutex_;
  std::unordered_map<FileId, Node::Pointer> node_map_;
  std::unordered_map<std::string, Node::Pointer> node_id_map_;
  std::unordered_map<FileId, std::unordered_set<FileId>> node_directory_;
  std::unordered_map<std::string, FileId> auth_node_;
  FileId next_;
  std::deque<RequestData> request_data_;
  std::deque<std::shared_ptr<IGenericRequest>> cancelled_request_;
  std::atomic_bool running_;
  IHttp::Pointer http_;
  std::string temporary_directory_;
  std::condition_variable_any cancelled_request_condition_;
  std::condition_variable_any request_data_condition_;
  std::future<void> cancelled_request_thread_;
  std::future<void> cleanup_;
};

}  // namespace cloudstorage

#endif  // FILE_SYSTEM_H
