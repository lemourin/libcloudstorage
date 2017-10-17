#include "FileSystem.h"

#include <json/json.h>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>

#include "CloudProvider/CloudProvider.h"
#include "ICloudStorage.h"
#include "Utility/Item.h"
#include "Utility/Utility.h"

const std::string AUTH_ITEM_ID = "NVap5sT9XY";

namespace cloudstorage {

using util::log;

namespace {

std::string authorize_file(const std::string& url) {
  std::stringstream stream;
  stream << "<html><script>window.location.href=\"" << url
         << "\";</script></html>";
  return stream.str();
}

IItem::Pointer auth_item(const std::string& url) {
  return util::make_unique<cloudstorage::Item>(
      "authorize.html", AUTH_ITEM_ID, authorize_file(url).size(),
      IItem::UnknownTimeStamp, IItem::FileType::Unknown);
}

std::string id(std::shared_ptr<ICloudProvider> p, IItem::Pointer i) {
  Json::Value json;
  if (p) json["p"] = p->name();
  json["i"] = i->filename() + i->id();
  return util::json::to_string(json);
}

}  // namespace

FileSystem::Node::Node() : inode_(), size_() {}

FileSystem::Node::Node(std::shared_ptr<ICloudProvider> p, IItem::Pointer item,
                       FileId inode, uint64_t size)
    : provider_(p), item_(item), inode_(inode), size_(size) {}

FileSystem::FileId FileSystem::Node::inode() const { return inode_; }

std::chrono::system_clock::time_point FileSystem::Node::timestamp() const {
  return item_->timestamp();
}

uint64_t FileSystem::Node::size() const { return size_; }

void FileSystem::Node::set_size(uint64_t size) { size_ = size; }

std::string FileSystem::Node::filename() const { return item_->filename(); }

IItem::FileType FileSystem::Node::type() const { return item_->type(); }

IItem::Pointer FileSystem::Node::item() const { return item_; }

std::shared_ptr<ICloudProvider> FileSystem::Node::provider() const {
  return provider_;
}

std::shared_ptr<IGenericRequest> FileSystem::Node::upload_request() const {
  return upload_request_;
}

void FileSystem::Node::set_upload_request(std::shared_ptr<IGenericRequest> r) {
  upload_request_ = r;
}

FileSystem::CreatedNode::CreatedNode(FileId parent, const std::string& filename,
                                     const std::string& cache_filename)
    : parent_(parent),
      filename_(filename),
      cache_filename_(cache_filename),
      store_(
          cache_filename,
          std::ios::in | std::ios::out | std::ios::binary | std::ios::trunc) {}

FileSystem::CreatedNode::~CreatedNode() {
  (void)std::remove(cache_filename_.c_str());
}

FileSystem::FileSystem(const std::vector<ProviderEntry>& provider,
                       IHttp::Pointer http,
                       const std::string& temporary_directory)
    : next_(1),
      running_(true),
      http_(std::move(http)),
      temporary_directory_(temporary_directory),
      cancelled_request_thread_(std::async(
          std::launch::async, std::bind(&FileSystem::cancelled, this))),
      cleanup_(std::async(std::launch::async,
                          std::bind(&FileSystem::cleanup, this))) {
  add(nullptr, util::make_unique<cloudstorage::Item>(
                   "/", "root", IItem::UnknownSize, IItem::UnknownTimeStamp,
                   IItem::FileType::Directory));
  std::unordered_set<FileId> root_directory;
  for (auto&& entry : provider) {
    IItem::Pointer item = util::make_unique<cloudstorage::Item>(
        entry.label_, entry.provider_->rootDirectory()->id(),
        IItem::UnknownSize, IItem::UnknownTimeStamp,
        IItem::FileType::Directory);
    root_directory.insert(add(entry.provider_, item)->inode());
    auth_node_[entry.label_] =
        add(entry.provider_, auth_item(entry.provider_->name()))->inode();
  }
  node_directory_[1] = root_directory;
}

FileSystem::~FileSystem() {
  running_ = false;
  request_data_condition_.notify_one();
  cancelled_request_condition_.notify_one();
  cancelled_request_thread_.wait();
  cleanup_.wait();
}

void FileSystem::cleanup() {
  while (running_) {
    std::unique_lock<mutex> lock(request_data_mutex_);
    request_data_condition_.wait(
        lock, [=]() { return !request_data_.empty() || !running_; });
    while (!request_data_.empty()) {
      {
        auto r = std::move(request_data_.front());
        request_data_.pop_front();
        lock.unlock();
        r.request_->finish();
      }
      lock.lock();
    }
  }
}

void FileSystem::cancelled() {
  while (running_) {
    std::unique_lock<mutex> lock(request_data_mutex_);
    cancelled_request_condition_.wait(
        lock, [=]() { return !cancelled_request_.empty() || !running_; });
    while (!cancelled_request_.empty()) {
      {
        auto r = std::move(cancelled_request_.front());
        cancelled_request_.pop_front();
        lock.unlock();
        r->cancel();
      }
      lock.lock();
    }
  }
}

void FileSystem::cancel(std::shared_ptr<IGenericRequest> r) {
  {
    std::unique_lock<mutex> lock(request_data_mutex_);
    cancelled_request_.push_back(r);
  }
  cancelled_request_condition_.notify_one();
}

void FileSystem::add(RequestData r) {
  std::lock_guard<mutex> lock(request_data_mutex_);
  request_data_.push_back(std::move(r));
  request_data_condition_.notify_one();
}

FileSystem::Node::Pointer FileSystem::add(std::shared_ptr<ICloudProvider> p,
                                          IItem::Pointer i) {
  std::lock_guard<mutex> lock(node_data_mutex_);
  auto it = node_id_map_.find(id(p, i));
  if (it == std::end(node_id_map_)) {
    auto idx = next_++;
    auto node = std::make_shared<Node>(p, i, idx, i->size());
    node_map_[idx] = node;
    node_id_map_[id(p, i)] = node;
    return node;
  } else
    return it->second;
}

void FileSystem::set(FileId idx, Node::Pointer node) {
  std::lock_guard<mutex> lock(node_data_mutex_);
  if (node->item()) {
    node_map_[idx] = node;
    node_id_map_[id(node->provider(), node->item())] = node;
  } else {
    auto it1 = node_map_.find(idx);
    if (it1 != std::end(node_map_)) {
      auto it2 =
          node_id_map_.find(id(it1->second->provider(), it1->second->item()));
      if (it2 != std::end(node_id_map_)) node_id_map_.erase(it2);
      node_map_.erase(it1);
    }
    auto it3 = node_directory_.find(idx);
    if (it3 != std::end(node_directory_)) node_directory_.erase(it3);
  }
}

IFileSystem::FileId FileSystem::mknod(FileId parent, const char* name) {
  std::lock_guard<mutex> lock(node_data_mutex_);
  auto p = get(parent);
  if (!p->provider()) return 0;
  auto idx = next_++;
  created_node_[idx] = util::make_unique<CreatedNode>(
      parent, name,
      temporary_directory_ + "cloudstorage" + std::to_string(idx));
  set(idx, std::make_shared<Node>(
               p->provider(),
               std::make_shared<Item>(name, "", 0, IItem::UnknownTimeStamp,
                                      IItem::FileType::Unknown),
               idx, 0));
  auto it = node_directory_.find(parent);
  if (it != node_directory_.end()) it->second.insert(idx);
  return idx;
}

FileSystem::Node::Pointer FileSystem::get(FileId node) {
  std::unique_lock<mutex> lock(node_data_mutex_);
  auto it = node_map_.find(node);
  if (it == node_map_.end())
    return std::make_shared<Node>();
  else
    return it->second;
}

void FileSystem::lookup(FileId parent_node, const std::string& name,
                        GetItemCallback cb) {
  readdir(parent_node, [=](EitherError<std::vector<INode::Pointer>> e) {
    if (auto lst = e.right()) {
      for (auto&& i : *lst)
        if (this->sanitize(i->filename()) == name) return cb(i);
      cb(Error{IHttpRequest::Bad, "not found"});
    } else {
      cb(e.left());
    }
  });
}

void FileSystem::getattr(FileId node, GetItemCallback cb) {
  auto n = get(node);
  if (n->item()) {
    if (n->type() != IItem::FileType::Directory &&
        n->size() == IItem::UnknownSize)
      get_url_async(n->provider(), n->item(), [=](EitherError<std::string> e) {
        if (auto url = e.right()) {
          http_->create(*url, "HEAD")
              ->send(
                  [=](IHttpRequest::Response response) {
                    if (IHttpRequest::isSuccess(response.http_code_)) {
                      if (response.headers_.find("content-length") ==
                          response.headers_.end())
                        return this->download_item_async(
                            n->provider(), n->item(), FullRange,
                            [=](EitherError<std::string> e) {
                              if (e.left()) return cb(e.left());
                              auto nnode = std::make_shared<Node>(
                                  n->provider(), n->item(), node,
                                  e.right()->size());
                              cb(std::static_pointer_cast<INode>(nnode));
                            });
                      auto size = std::atoll(
                          response.headers_["content-length"].c_str());
                      auto nnode =
                          std::make_shared<Node>(n->provider(), n->item(), node,
                                                 static_cast<uint64_t>(size));
                      this->set(node, nnode);
                      cb(std::static_pointer_cast<INode>(nnode));
                    } else
                      cb(Error{response.http_code_,
                               std::static_pointer_cast<std::stringstream>(
                                   response.error_stream_)
                                   ->str()});
                  },
                  std::make_shared<std::stringstream>(),
                  std::make_shared<std::stringstream>(),
                  std::make_shared<std::stringstream>());
        } else
          cb(e.left());
      });
    else
      cb(std::static_pointer_cast<INode>(n));
  } else {
    cb(Error{IHttpRequest::Bad, ""});
  }
}

void FileSystem::getattr(const std::string& path, GetItemCallback callback) {
  return get_path(1, path, callback);
}

void FileSystem::get_path(FileId node, const std::string& path,
                          GetItemCallback callback) {
  if (path.empty() || path == "/") return getattr(node, callback);
  auto it = path.find_first_of('/', 1);
  auto filename =
      it == std::string::npos ? path.substr(1) : path.substr(1, it - 1);
  auto rest = it == std::string::npos ? "/" : path.substr(it);
  lookup(node, filename, [=](EitherError<INode> e) {
    if (e.left()) return callback(e.left());
    this->get_path(e.right()->inode(), rest, callback);
  });
}

void FileSystem::write(FileId inode, const char* data, uint32_t size,
                       uint64_t offset, WriteDataCallback callback) {
  std::lock_guard<mutex> lock(node_data_mutex_);
  auto it = created_node_.find(inode);
  if (it == created_node_.end())
    return callback(Error{IHttpRequest::ServiceUnavailable,
                          "can't write over already present files"});
  const auto& n = it->second;
  n->store_.seekp(offset);
  n->store_.write(data, size);
  if (!n->store_)
    return callback(0);
  else
    return callback(size);
}

void FileSystem::readdir(FileId node, ListDirectoryCallback cb) {
  {
    std::lock_guard<mutex> lock(node_data_mutex_);
    auto it = node_directory_.find(node);
    if (it != std::end(node_directory_)) {
      std::vector<INode::Pointer> ret;
      for (auto&& r : it->second) ret.push_back(get(r));
      return cb(ret);
    }
  }
  auto nd = get(node);
  if (nd->provider() == nullptr) return cb(Error{IHttpRequest::Bad, ""});
  list_directory_async(
      nd->provider(), nd->item(),
      [=](EitherError<std::vector<IItem::Pointer>> e) {
        if (auto lst = e.right()) {
          std::unordered_set<FileId> ret;
          for (auto&& i : *lst)
            ret.insert(this->add(nd->provider(), i)->inode());
          {
            std::lock_guard<mutex> lock(node_data_mutex_);
            node_directory_[node] = ret;
          }
          std::vector<INode::Pointer> nodes;
          for (auto&& r : ret) nodes.push_back(this->get(r));
          cb(nodes);
        } else {
          auto item = auth_item(nd->provider()->authorizeLibraryUrl());
          cb(std::vector<INode::Pointer>(
              1, std::make_shared<Node>(nd->provider(), item,
                                        auth_node_[nd->provider()->name()],
                                        item->size())));
        }
      });
}

void FileSystem::read(FileId node, size_t offset, size_t sz,
                      DownloadItemCallback cb) {
  getattr(node, [=](EitherError<INode> e) {
    if (e.left()) return cb(e.left());
    auto nd = std::static_pointer_cast<Node>(e.right());
    if (nd->size() == IItem::UnknownSize || nd->size() == 0 || !nd->provider())
      return cb(std::string());
    if (nd->item()->id() == AUTH_ITEM_ID) {
      auto data = authorize_file(nd->provider()->authorizeLibraryUrl());
      auto start = std::min<size_t>(offset, data.size() - 1);
      auto size = std::min<size_t>(data.size() - start, sz);
      auto d = data.substr(start, size);
      return cb(d);
    }
    auto fit = [=](Range r) {
      r.start_ =
          std::min<size_t>(r.start_, static_cast<size_t>(nd->size() - 1));
      r.size_ =
          std::min<size_t>(r.size_, static_cast<size_t>(nd->size() - r.start_));
      return r;
    };
    Range range = fit({offset, sz});
    std::unique_lock<mutex> lock(nd->mutex_);
    auto inside = [=](Range r1, Range r2) {
      r1 = fit(r1);
      r2 = fit(r2);
      return r1.start_ >= r2.start_ &&
             r1.start_ + r1.size_ <= r2.start_ + r2.size_;
    };
    auto download = [=](Range range) {
      for (auto&& download_range : nd->pending_download_)
        if (inside(range, download_range)) return;
      range = fit({range.start_, std::max<size_t>(range.size_, READ_AHEAD)});
      nd->pending_download_.push_back(range);
      download_item_async(
          nd->provider(), nd->item(), range, [=](EitherError<std::string> e) {
            std::unique_lock<mutex> lock(nd->mutex_);
            auto requests = nd->read_request_;
            for (auto&& read : requests)
              if (inside(read.range_, range)) {
                if (e.left())
                  read.callback_(e.left());
                else
                  read.callback_(e.right()->substr(
                      read.range_.start_ - range.start_, read.range_.size_));
                auto it = std::find(nd->read_request_.begin(),
                                    nd->read_request_.end(), read);
                if (it != nd->read_request_.end()) nd->read_request_.erase(it);
              }
            auto it = std::find(nd->pending_download_.begin(),
                                nd->pending_download_.end(), range);
            if (it != nd->pending_download_.end())
              nd->pending_download_.erase(it);
            if (e.right()) {
              nd->chunk_.push_back({range, std::move(*e.right())});
              if (nd->chunk_.size() >= CACHED_CHUNK_COUNT)
                nd->chunk_.pop_front();
            }
          });
    };
    bool read_ahead = true;
    for (auto&& chunk : nd->chunk_)
      if (inside(Range{range.start_ + READ_AHEAD / 2, READ_AHEAD / 2},
                 chunk.range_))
        read_ahead = false;
    if (read_ahead) download(Range{range.start_ + READ_AHEAD / 2, range.size_});
    for (auto&& chunk : nd->chunk_)
      if (inside(range, chunk.range_))
        return cb(chunk.data_.substr(range.start_ - chunk.range_.start_,
                                     range.size_));
    nd->read_request_.push_back({range, cb});
    download(range);
  });
}

void FileSystem::invalidate(FileId root) {
  std::lock_guard<mutex> lock(node_data_mutex_);
  auto it = node_directory_.find(root);
  if (it != node_directory_.end()) {
    for (auto&& n : it->second) {
      invalidate(n);
      set(n, std::make_shared<Node>());
    }
    node_directory_.erase(it);
  }
}

void FileSystem::rename(FileId parent, const char* name, FileId newparent,
                        const char* newname, RenameItemCallback callback) {
  lookup(parent, name, [=](EitherError<INode> e) {
    if (e.left()) return callback(e.left());
    auto parent_node = this->get(parent);
    auto destination_node = this->get(newparent);
    if (!parent_node->provider() || !destination_node->provider())
      return callback(Error{IHttpRequest::Failure, "invalid provider"});
    if (parent_node->provider()->name() != destination_node->provider()->name())
      return callback(Error{IHttpRequest::ServiceUnavailable,
                            "can't move files between providers"});
    auto parent_item = parent_node->item();
    auto destination_item = destination_node->item();
    auto node = std::static_pointer_cast<Node>(e.right());
    auto p = node->provider();
    if (!p) return callback(Error{IHttpRequest::ServiceUnavailable, ""});
    this->rename_async(
        p, node->item(), parent_item, destination_item, newname,
        [=](EitherError<IItem> e) {
          if (e.right()) {
            std::lock_guard<mutex> lock(node_data_mutex_);
            this->invalidate(node->inode());
            auto& old_lst = node_directory_[parent];
            auto it = std::find(old_lst.begin(), old_lst.end(), node->inode());
            if (it != std::end(old_lst)) old_lst.erase(it);
            auto nit = node_directory_.find(newparent);
            if (nit != std::end(node_directory_))
              nit->second.insert(node->inode());
            this->set(node->inode(),
                      std::make_shared<Node>(p, e.right(), node->inode(),
                                             node->size()));
          }
          callback(e);
        });
  });
}

void FileSystem::remove(FileId parent, const char* name,
                        DeleteItemCallback callback) {
  auto update_lists = [=](Node::Pointer node) {
    std::lock_guard<mutex> lock(node_data_mutex_);
    auto it = node_directory_.find(parent);
    if (it != node_directory_.end()) {
      auto d = it->second.find(node->inode());
      if (d != it->second.end()) it->second.erase(d);
    }
  };
  auto remove_file = [=](Node::Pointer node) {
    std::lock_guard<mutex> lock(node_data_mutex_);
    if (node->upload_request()) {
      this->cancel(node->upload_request());
      update_lists(node);
      return callback(nullptr);
    }
    if (!node->provider())
      return callback(Error{IHttpRequest::ServiceUnavailable, ""});
    this->add({node->provider(),
               node->provider()->deleteItemAsync(node->item(),
                                                 [=](EitherError<void> e) {
                                                   if (e.left())
                                                     return callback(e.left());
                                                   update_lists(node);
                                                   callback(nullptr);
                                                 })});
  };
  lookup(parent, name, [=](EitherError<INode> e) {
    if (e.left()) return callback(e.left());
    auto node = std::static_pointer_cast<Node>(e.right());
    if (node->type() == IItem::FileType::Directory) {
      this->readdir(node->inode(),
                    [=](EitherError<std::vector<INode::Pointer>> e) {
                      if (e.left()) return callback(e.left());
                      if (!e.right()->empty())
                        return callback(Error{NotEmpty, "not empty"});
                      remove_file(node);
                    });
    } else
      remove_file(node);
  });
}

void FileSystem::release(FileId inode, DeleteItemCallback cb) {
  std::lock_guard<mutex> lock(node_data_mutex_);
  auto it = created_node_.find(inode);
  if (it == created_node_.end()) {
    auto node = get(inode);
    std::lock_guard<mutex> lock(node->mutex_);
    node->chunk_.clear();
    return cb(nullptr);
  }
  auto node = std::move(it->second);
  created_node_.erase(it);
  auto parent_node = get(node->parent_);
  auto p = parent_node->provider();
  if (!p) return cb(Error{IHttpRequest::ServiceUnavailable, ""});
  class UploadCallback : public IUploadFileCallback {
   public:
    UploadCallback(FileSystem* ctx, std::shared_ptr<ICloudProvider> provider,
                   FileId inode, CreatedNode::Pointer node,
                   DeleteItemCallback cb)
        : fuse_(ctx),
          provider_(provider),
          inode_(inode),
          node_(std::move(node)),
          callback_(cb) {
      node_->store_.seekg(0, std::ios::end);
      size_ = node_->store_.tellg();
      node_->store_.seekg(std::ios::beg);
    }

    void reset() override { node_->store_.seekg(std::ios::beg); }

    uint32_t putData(char* data, uint32_t maxlength) override {
      node_->store_.read(data, maxlength);
      return node_->store_.gcount();
    }

    uint64_t size() override { return size_; }

    void done(EitherError<IItem> e) override {
      if (e.left()) return callback_(e.left());
      std::lock_guard<mutex> lock(fuse_->node_data_mutex_);
      fuse_->set(inode_, std::make_shared<Node>(provider_, e.right(), inode_,
                                                e.right()->size()));
      callback_(nullptr);
    }

    void progress(uint64_t, uint64_t now) override {
      std::lock_guard<mutex> lock(fuse_->node_data_mutex_);
      auto node = fuse_->get(inode_);
      node->set_size(now);
      fuse_->set(inode_, node);
    }

   private:
    FileSystem* fuse_;
    std::shared_ptr<ICloudProvider> provider_;
    FileId inode_;
    CreatedNode::Pointer node_;
    DeleteItemCallback callback_;
    uint64_t size_;
  };
  auto filename = node->filename_;
  std::shared_ptr<IGenericRequest> upload_request = p->uploadFileAsync(
      parent_node->item(), filename,
      util::make_unique<UploadCallback>(this, p, inode, std::move(node), cb));
  auto fake_node = get(inode);
  fake_node->set_upload_request(upload_request);
  set(inode, fake_node);
  add({p, upload_request});
}

void FileSystem::mkdir(FileId parent, const char* name,
                       GetItemCallback callback) {
  auto node = get(parent);
  auto p = node->provider();
  if (!p) return callback(Error{IHttpRequest::Bad, ""});
  add({p,
       p->createDirectoryAsync(node->item(), name, [=](EitherError<IItem> e) {
         if (e.left()) return callback(e.left());
         std::lock_guard<mutex> lock(node_data_mutex_);
         auto node = this->add(p, e.right());
         auto it = node_directory_.find(parent);
         if (it != node_directory_.end()) it->second.insert(node->inode());
         callback(std::static_pointer_cast<INode>(node));
       })});
}

void FileSystem::rename_async(std::shared_ptr<ICloudProvider> p,
                              IItem::Pointer item, IItem::Pointer parent,
                              IItem::Pointer destination, const char* name,
                              RenameItemCallback callback) {
  if (!p || !item || !parent || !destination)
    return callback(Error{IHttpRequest::ServiceUnavailable, ""});
  auto move = [=](IItem::Pointer item) {
    if (parent != destination)
      this->add({p, p->moveItemAsync(item, destination, callback)});
    else
      callback(item);
  };
  if (sanitize(item->filename()) != name) {
    this->add({p, p->renameItemAsync(item, name, [=](EitherError<IItem> e) {
                 if (e.left()) return callback(e.left());
                 move(e.right());
               })});
  } else {
    move(item);
  }
}

void FileSystem::list_directory_async(std::shared_ptr<ICloudProvider> p,
                                      IItem::Pointer i,
                                      cloudstorage::ListDirectoryCallback cb) {
  if (!p || !i) return cb(Error{IHttpRequest::ServiceUnavailable, ""});
  add({p, p->listDirectoryAsync(i, cb)});
}

void FileSystem::download_item_async(std::shared_ptr<ICloudProvider> p,
                                     IItem::Pointer item, Range range,
                                     DownloadItemCallback cb) {
  if (!p || !item) return cb(Error{IHttpRequest::ServiceUnavailable, ""});
  class Callback : public IDownloadFileCallback {
   public:
    Callback(DownloadItemCallback cb, size_t size)
        : start_(std::chrono::system_clock::now()),
          callback_(cb),
          size_(size) {}

    void receivedData(const char* data, uint32_t length) override {
      buffer_ += std::string(data, length);
    }

    void done(EitherError<void> e) override {
      log("access time",
          std::chrono::duration<double>(std::chrono::system_clock::now() -
                                        start_)
              .count());
      if (e.left()) return callback_(e.left());
      buffer_.resize(std::min<size_t>(buffer_.size(), size_));
      callback_(buffer_);
    }

    void progress(uint64_t, uint64_t) override {}

   private:
    std::chrono::system_clock::time_point start_;
    std::string buffer_;
    DownloadItemCallback callback_;
    size_t size_;
  };
  log("requesting", range.start_, "-", range.start_ + range.size_ - 1);
  add({p,
       p->downloadFileAsync(item, util::make_unique<Callback>(cb, range.size_),
                            range)});
}

void FileSystem::get_url_async(std::shared_ptr<ICloudProvider> p,
                               IItem::Pointer i, GetItemUrlCallback cb) {
  add({p, p->getItemUrlAsync(i, cb)});
}

std::string FileSystem::sanitize(const std::string& name) {
  const std::string forbidden = "~\"#%&*:<>?/\\{|}";
  std::string res;
  for (auto&& c : name)
    if (forbidden.find(c) == std::string::npos) res += c;
  return res;
}

IFileSystem::Pointer IFileSystem::create(
    const std::vector<ProviderEntry>& p, IHttp::Pointer http,
    const std::string& temporary_directory) {
  return util::make_unique<FileSystem>(p, std::move(http), temporary_directory);
}

}  // namespace cloudstorage
