#ifndef FUSE_COMMON_H
#define FUSE_COMMON_H

#ifdef WITH_FUSE
#define FUSE_USE_VERSION 31
#include <fuse_lowlevel.h>
#elif WITH_LEGACY_FUSE
#define FUSE_USE_VERSION 27
#include <fuse.h>
#endif

#ifndef FUSE_STAT
#define FUSE_STAT stat
#endif

#include <json/json.h>
#include <future>
#include "IFileSystem.h"

namespace cloudstorage {

class HttpWrapper : public IHttp {
 public:
  HttpWrapper(std::shared_ptr<IHttp> http) : http_(http) {}
  IHttpRequest::Pointer create(const std::string &url,
                               const std::string &method,
                               bool follow_redirect) const override;

 private:
  std::shared_ptr<IHttp> http_;
};

struct HttpServerData {
  std::string code_;
  std::string state_;
};

class HttpServerCallback : public IHttpServer::ICallback {
 public:
  HttpServerCallback(std::promise<HttpServerData> &p) : promise_(p) {}

  IHttpServer::IResponse::Pointer handle(
      const IHttpServer::IRequest &request) override;

 private:
  std::promise<HttpServerData> &promise_;
};

struct FUSE_STAT item_to_stat(IFileSystem::INode::Pointer i);
cloudstorage::ICloudProvider::Pointer create(std::shared_ptr<IHttp> http,
                                             std::string temporary_directory,
                                             Json::Value config);
std::vector<cloudstorage::IFileSystem::ProviderEntry> providers(
    const Json::Value &data, std::shared_ptr<IHttp> http,
    const std::string &temporary_directory);

std::string default_temporary_directory();
std::string default_home_directory();

int fuse_run(int argc, char **argv);

}  // namespace cloudstorage

#endif  // FUSE_COMMON_H
