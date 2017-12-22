/*****************************************************************************
 * ICrypto : ICrypto interface
 *
 *****************************************************************************
 * Copyright (C) 2016-2016 VideoLAN
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

#ifndef ICRYPTO_H
#define ICRYPTO_H

#include <memory>
#include <string>

namespace cloudstorage {

/**
 * Provides cryptographic methods.
 */
class ICrypto {
 public:
  using Pointer = std::unique_ptr<ICrypto>;

  virtual ~ICrypto() = default;

  /**
   * Computes SHA256 hash.
   *
   * @param message
   * @return SHA256 hash of message
   */
  virtual std::string sha256(const std::string& message) = 0;

  /**
   * Computes HMAC-SHA256
   * @param key
   * @param message
   * @return HMAC-SHA256 hash
   */
  virtual std::string hmac_sha256(const std::string& key,
                                  const std::string& message) = 0;
  /**
   * Computes HMAC-SHA1
   * @param key
   * @param message
   * @return HMAC-SHA1 hash
   */
  virtual std::string hmac_sha1(const std::string& key,
                                const std::string& message) = 0;

  /**
   * Converts hash to hex string.
   *
   * @param hash
   * @return hex string
   */
  virtual std::string hex(const std::string& hash) = 0;

  static ICrypto::Pointer create();
};

}  // namespace cloudstorage

#endif  // ICRYPTO_H
