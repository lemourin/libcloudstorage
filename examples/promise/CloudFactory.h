/*****************************************************************************
 * CloudFactory.h
 *
 *****************************************************************************
 * Copyright (C) 2016-2019 VideoLAN
 *
 * Authors: Paweł Wegner <pawel.wegner95@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "CloudAccess.h"
#include "CloudEventLoop.h"
#include "ICloudStorage.h"

#include <atomic>

namespace cloudstorage {

class CloudFactory {
 public:
  struct InitData {
    std::string base_url_;
    IHttp::Pointer http_;
    IHttpServerFactory::Pointer http_server_factory_;
    ICrypto::Pointer crypto_;
    IThreadPool::Pointer thread_pool_;
  };

  struct ProviderInitData {
    std::string token_;
    ICloudProvider::Permission permission_;
    ICloudProvider::Hints hints_;
  };

  CloudFactory(CloudEventLoop* event_loop, InitData&&);
  virtual ~CloudFactory();

  CloudAccess create(const std::string& provider_name,
                     const ProviderInitData&) const;

  void add(std::unique_ptr<IGenericRequest>&&);
  void invoke(const std::function<void()>&);

  virtual void onCloudTokenReceived(const std::string& provider,
                                    const EitherError<Token>&);

  virtual void onCloudCreated(std::shared_ptr<CloudAccess> cloud);
  virtual void onCloudRemoved(std::shared_ptr<CloudAccess> cloud);

  std::string authorizationUrl(const std::string& provider) const;
  std::vector<std::string> availableProviders() const;

  void onCloudRemoved(const ICloudProvider&);

  void dump(std::ostream& stream);
  void load(std::istream& stream);

  std::vector<std::shared_ptr<CloudAccess>> providers() const;

 private:
  std::string base_url_;
  std::shared_ptr<IHttp> http_;
  std::shared_ptr<IHttpServerFactory> http_server_factory_;
  std::shared_ptr<ICrypto> crypto_;
  std::shared_ptr<IThreadPool> thread_pool_;
  ICloudStorage::Pointer cloud_storage_;
  mutable std::atomic_uint64_t provider_index_;
  std::vector<IHttpServer::Pointer> http_server_handles_;
  std::unordered_map<uint64_t, std::shared_ptr<CloudAccess>> cloud_access_;
  std::shared_ptr<priv::LoopImpl> loop_;
};

}  // namespace cloudstorage
