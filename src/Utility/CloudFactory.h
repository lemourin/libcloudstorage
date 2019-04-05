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

#include <json/json.h>
#include <atomic>
#include <condition_variable>

#include "ICloudFactory.h"

class ServerWrapperFactory;

namespace cloudstorage {

class CloudFactory : public ICloudFactory {
 public:
  CloudFactory(InitData&&);
  ~CloudFactory() override;

  std::shared_ptr<ICloudAccess> create(const std::string& provider_name,
                                       const ProviderInitData&) override;
  void remove(const std::shared_ptr<ICloudAccess>&) override;

  void add(std::unique_ptr<IGenericRequest>&&);
  void invoke(const std::function<void()>&);

  void onCloudTokenReceived(const std::string& provider,
                            const EitherError<Token>&);
  void onCloudAuthenticationCodeReceived(const std::string& provider,
                                         const std::string& code);

  void onCloudCreated(std::shared_ptr<CloudAccess> cloud);
  void onCloudRemoved(std::shared_ptr<CloudAccess> cloud);

  std::string authorizationUrl(const std::string& provider) const override;
  std::string pretty(const std::string& provider) const override;
  std::vector<std::string> availableProviders() const override;
  bool httpServerAvailable() const override;

  void onCloudRemoved(const ICloudProvider&);

  bool dumpAccounts(std::ostream& stream) override;
  bool dumpAccounts(std::ostream&& stream) override;

  bool loadAccounts(std::istream& stream) override;
  bool loadAccounts(std::istream&& stream) override;

  bool loadConfig(std::istream& stream) override;
  bool loadConfig(std::istream&& stream) override;

  void processEvents() override;

  int exec() override;
  void quit() override;

  std::vector<std::shared_ptr<ICloudAccess>> providers() const override;

  CloudAccess createImpl(const std::string& provider_name,
                         const ProviderInitData&) const;

  void onEventsAdded();

 private:
  std::shared_ptr<ICallback> callback_;
  CloudEventLoop event_loop_;
  std::string base_url_;
  std::shared_ptr<IHttp> http_;
  std::shared_ptr<ServerWrapperFactory> http_server_factory_;
  std::shared_ptr<ICrypto> crypto_;
  std::shared_ptr<IThreadPool> thread_pool_;
  ICloudStorage::Pointer cloud_storage_;
  mutable std::atomic_uint64_t provider_index_;
  std::vector<IHttpServer::Pointer> http_server_handles_;
  std::unordered_map<uint64_t, std::shared_ptr<CloudAccess>> cloud_access_;
  std::shared_ptr<priv::LoopImpl> loop_;
  Json::Value config_;
  std::mutex mutex_;
  std::condition_variable empty_condition_;
  int events_ready_ = false;
  bool quit_ = false;
};

}  // namespace cloudstorage
