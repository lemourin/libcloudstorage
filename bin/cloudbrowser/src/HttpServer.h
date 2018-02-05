#ifndef HTTPSERVER_H
#define HTTPSERVER_H

#include <mutex>

#include "ICloudProvider.h"

class DispatchCallback : public cloudstorage::IHttpServer::ICallback {
 public:
  cloudstorage::IHttpServer::IResponse::Pointer handle(
      const cloudstorage::IHttpServer::IRequest&) override;

  void add(const std::string&, ICallback::Pointer);
  void remove(const std::string&);
  ICallback::Pointer callback(const std::string&) const;

 private:
  mutable std::mutex lock_;
  std::unordered_map<std::string, cloudstorage::IHttpServer::ICallback::Pointer>
      client_callback_;
};

class HttpServerWrapper : public cloudstorage::IHttpServer {
 public:
  HttpServerWrapper(std::shared_ptr<DispatchCallback>,
                    cloudstorage::IHttpServer::ICallback::Pointer callback,
                    const std::string& session);
  ~HttpServerWrapper();

  ICallback::Pointer callback() const override;

 private:
  std::shared_ptr<DispatchCallback> dispatch_;
  std::string session_;
};

class ServerWrapperFactory : public cloudstorage::IHttpServerFactory {
 public:
  ServerWrapperFactory(std::shared_ptr<cloudstorage::IHttpServerFactory>);
  cloudstorage::IHttpServer::Pointer create(
      cloudstorage::IHttpServer::ICallback::Pointer,
      const std::string& session_id, cloudstorage::IHttpServer::Type) override;

  bool serverAvailable() const { return http_server_ != nullptr; }

 private:
  std::shared_ptr<DispatchCallback> callback_;
  std::shared_ptr<cloudstorage::IHttpServer> http_server_;
};

std::string first_url_part(const std::string& url);

#endif  // HTTPSERVER_H
