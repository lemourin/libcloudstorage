#include "HttpServer.h"

#include "IHttpServer.h"

#include "Utility/Utility.h"

using namespace cloudstorage;

std::string first_url_part(const std::string& url) {
  auto idx = url.find_first_of('/', 1);
  if (idx == std::string::npos)
    return url.substr(1);
  else
    return std::string(url.begin() + 1, url.begin() + idx);
}

IHttpServer::IResponse::Pointer DispatchCallback::handle(
    const IHttpServer::IRequest& r) {
  auto state = first_url_part(r.url());
  auto cb = callback(state);
  if (!cb)
    return util::response_from_string(r, IHttpRequest::Bad, {},
                                      "invalid state");
  return cb->handle(r);
}

void DispatchCallback::add(const std::string& ssid,
                           IHttpServer::ICallback::Pointer cb) {
  std::lock_guard<std::mutex> lock(lock_);
  client_callback_[ssid] = std::move(cb);
}

void DispatchCallback::remove(const std::string& ssid) {
  std::lock_guard<std::mutex> lock(lock_);
  client_callback_.erase(ssid);
}

IHttpServer::ICallback::Pointer DispatchCallback::callback(
    const std::string& ssid) const {
  std::lock_guard<std::mutex> lock(lock_);
  auto it = client_callback_.find(ssid);
  if (it == client_callback_.end())
    return nullptr;
  else
    return it->second;
}

HttpServerWrapper::HttpServerWrapper(std::shared_ptr<DispatchCallback> d,
                                     IHttpServer::ICallback::Pointer callback,
                                     const std::string& session)
    : dispatch_(std::move(d)), session_(session) {
  dispatch_->add(session, std::move(callback));
}

HttpServerWrapper::~HttpServerWrapper() { dispatch_->remove(session_); }

IHttpServer::ICallback::Pointer HttpServerWrapper::callback() const {
  return dispatch_->callback(session_);
}

ServerWrapperFactory::ServerWrapperFactory(IHttpServerFactory* factory)
    : callback_(std::make_shared<DispatchCallback>()),
      http_server_(
          factory->create(callback_, "", IHttpServer::Type::Authorization)) {}

IHttpServer::Pointer ServerWrapperFactory::create(
    IHttpServer::ICallback::Pointer callback, const std::string& session_id,
    IHttpServer::Type) {
  return util::make_unique<HttpServerWrapper>(callback_, callback, session_id);
}
