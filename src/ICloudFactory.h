/*****************************************************************************
 * ICloudFactory.h
 *
 *****************************************************************************
 * Copyright (C) 2019 VideoLAN
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
#ifndef ICLOUDFACTORY_H
#define ICLOUDFACTORY_H

#include "ICloudAccess.h"
#include "ICloudProvider.h"
#include "ICloudStorage.h"

namespace cloudstorage {

class CLOUDSTORAGE_API IException : public std::exception {
 public:
  virtual int code() const = 0;
};

class CLOUDSTORAGE_API IThreadPoolFactory {
 public:
  using Pointer = std::unique_ptr<IThreadPoolFactory>;

  virtual ~IThreadPoolFactory() = default;
  virtual IThreadPool::Pointer create(uint32_t requested_thread_count) = 0;
};

class CLOUDSTORAGE_API ICloudFactory {
 public:
  class ICallback {
   public:
    using Pointer = std::shared_ptr<ICallback>;

    virtual ~ICallback() = default;

    virtual void onCloudAuthenticationCodeExchangeFailed(
        const std::string& /* provider */, const IException&) {}
    virtual void onCloudAuthenticationCodeReceived(
        const std::string& /* provider */, const std::string&) {}
    virtual void onCloudCreated(const std::shared_ptr<ICloudAccess>&) {}
    virtual void onCloudRemoved(const std::shared_ptr<ICloudAccess>&) {}
    virtual void onEventsAdded() {}
  };

  struct InitData {
    std::string base_url_;
    IHttp::Pointer http_;
    IHttpServerFactory::Pointer http_server_factory_;
    ICrypto::Pointer crypto_;
    IThreadPoolFactory::Pointer thread_pool_factory_;
    ICallback::Pointer callback_;
  };

  struct ProviderInitData {
    std::string token_;
    ICloudProvider::Permission permission_;
    ICloudProvider::Hints hints_;
  };

  virtual ~ICloudFactory() = default;

  virtual std::shared_ptr<ICloudAccess> create(const std::string& provider_name,
                                               const ProviderInitData&) = 0;
  virtual void remove(const std::shared_ptr<ICloudAccess>&) = 0;

  virtual std::string authorizationUrl(
      const std::string& provider,
      const ProviderInitData& = ProviderInitData{
          "", ICloudProvider::Permission::ReadWrite, {}}) const = 0;
  virtual std::string pretty(const std::string& provider) const = 0;
  virtual bool httpServerAvailable() const = 0;

  virtual void processEvents() = 0;

  virtual std::vector<std::string> availableProviders() const = 0;
  virtual std::vector<std::shared_ptr<ICloudAccess>> providers() const = 0;

  virtual bool dumpAccounts(std::ostream& stream) = 0;
  virtual bool dumpAccounts(std::ostream&& stream) = 0;

  virtual bool loadAccounts(std::istream& stream) = 0;
  virtual bool loadAccounts(std::istream&& stream) = 0;

  virtual bool loadConfig(std::istream& stream) = 0;
  virtual bool loadConfig(std::istream&& stream) = 0;

  virtual int exec() = 0;
  virtual void quit() = 0;

  static void initialize(void* javaVM);
  static std::unique_ptr<ICloudFactory> create(const ICallback::Pointer&);
  static std::unique_ptr<ICloudFactory> create(InitData&&);
};

}  // namespace cloudstorage

#endif  // ICLOUDFACTORY_H
