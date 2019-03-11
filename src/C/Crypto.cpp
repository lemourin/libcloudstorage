/*****************************************************************************
 * Crypto.cpp
 *
 *****************************************************************************
 * Copyright (C) 2016-2018 VideoLAN
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
#include "Crypto.h"

#include <cstdlib>

#include "ICrypto.h"
#include "Utility/Utility.h"

using namespace cloudstorage;

cloud_crypto *cloud_crypto_create_default() {
  return reinterpret_cast<cloud_crypto *>(
      new std::unique_ptr<ICrypto>(ICrypto::create()));
}

void cloud_crypto_release(cloud_crypto *d) {
  delete reinterpret_cast<std::unique_ptr<ICrypto> *>(d);
}

cloud_crypto *cloud_crypto_create(cloud_crypto_operations *d, void *userdata) {
  struct Crypto : public ICrypto {
    Crypto(cloud_crypto_operations ops, void *userdata)
        : operations_(ops), userdata_(userdata) {}

    ~Crypto() override { operations_.release(userdata_); }

    std::string sha256(const std::string &message) override {
      auto result = operations_.sha256(
          {reinterpret_cast<const void *>(message.c_str()), message.size()},
          userdata_);
      auto r = std::string(reinterpret_cast<const char *>(result.data),
                           result.length);
      free(const_cast<void *>(result.data));
      return r;
    }
    std::string hmac_sha256(const std::string &key,
                            const std::string &message) override {
      auto result = operations_.hmac_sha256(
          {reinterpret_cast<const void *>(key.c_str()), key.size()},
          {reinterpret_cast<const void *>(message.c_str()), message.size()},
          userdata_);
      auto r = std::string(reinterpret_cast<const char *>(result.data),
                           result.length);
      free(const_cast<void *>(result.data));
      return r;
    }
    std::string hmac_sha1(const std::string &key,
                          const std::string &message) override {
      auto result = operations_.hmac_sha1(
          {reinterpret_cast<const void *>(key.c_str()), key.size()},
          {reinterpret_cast<const void *>(message.c_str()), message.size()},
          userdata_);
      auto r = std::string(reinterpret_cast<const char *>(result.data),
                           result.length);
      free(const_cast<void *>(result.data));
      return r;
    }
    std::string hex(const std::string &hash) override {
      auto result = operations_.hex(
          {reinterpret_cast<const void *>(hash.c_str()), hash.size()},
          userdata_);
      auto r = std::string(reinterpret_cast<const char *>(result.data),
                           result.length);
      free(const_cast<void *>(result.data));
      return r;
    }

    static std::string to_string(struct cloud_array array) {
      return std::string(reinterpret_cast<const char *>(array.data),
                         array.length);
    }

    cloud_crypto_operations operations_;
    void *userdata_;
  };
  return reinterpret_cast<cloud_crypto *>(
      util::make_unique<Crypto>(*d, userdata).release());
}
