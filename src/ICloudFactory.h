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

class IException : public std::exception {
 public:
  virtual int code() const = 0;
};

class ICloudFactory {
 public:
  class ICallback {
   public:
    using Pointer = std::shared_ptr<ICallback>;

    virtual ~ICallback() = default;

    virtual void onCloudTokenReceived(const std::string& provider,
                                      const EitherError<Token>&) = 0;
    virtual void onCloudCreated(std::shared_ptr<ICloudAccess> cloud) = 0;
    virtual void onCloudRemoved(std::shared_ptr<ICloudAccess> cloud) = 0;
    virtual void onEventsAdded() = 0;
  };

  struct InitData {
    std::string base_url_;
    IHttp::Pointer http_;
    IHttpServerFactory::Pointer http_server_factory_;
    ICrypto::Pointer crypto_;
    IThreadPool::Pointer thread_pool_;
    ICallback::Pointer callback_;
  };

  struct ProviderInitData {
    std::string token_;
    ICloudProvider::Permission permission_;
    ICloudProvider::Hints hints_;
  };

  virtual ~ICloudFactory() = default;

  virtual std::unique_ptr<ICloudAccess> create(
      const std::string& provider_name, const ProviderInitData&) const = 0;

  virtual std::string authorizationUrl(const std::string& provider) const = 0;

  virtual void processEvents() = 0;

  virtual std::vector<std::string> availableProviders() const = 0;
  virtual std::vector<std::shared_ptr<ICloudAccess>> providers() const = 0;

  virtual bool dumpAccounts(std::ostream&& stream) = 0;
  virtual bool loadAccounts(std::istream&& stream) = 0;
  virtual bool loadConfig(std::istream&& stream) = 0;

  virtual int exec() = 0;
  virtual void quit() = 0;

  static std::unique_ptr<ICloudFactory> create(const ICallback::Pointer&);
  static std::unique_ptr<ICloudFactory> create(InitData&&);
};

}  // namespace cloudstorage

#endif  // ICLOUDFACTORY_H
