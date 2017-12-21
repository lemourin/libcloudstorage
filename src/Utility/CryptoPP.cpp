/*****************************************************************************
 * CryptoPP.cpp :
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

#ifdef WITH_CRYPTOPP

#include "CryptoPP.h"

#include <cryptopp/cryptlib.h>
#include <cryptopp/hex.h>
#include <cryptopp/hmac.h>
#include <cryptopp/sha.h>

namespace cloudstorage {

std::string CryptoPP::sha256(const std::string& message) {
  ::CryptoPP::SHA256 hash;
  std::string result;
  ::CryptoPP::StringSource(
      message, true,
      new ::CryptoPP::HashFilter(hash, new ::CryptoPP::StringSink(result)));
  return result;
}

std::string CryptoPP::hmac_sha256(const std::string& key,
                                  const std::string& message) {
  std::string mac;
  ::CryptoPP::HMAC<::CryptoPP::SHA256> hmac((byte*)key.c_str(), key.length());
  ::CryptoPP::StringSource(
      message, true,
      new ::CryptoPP::HashFilter(hmac, new ::CryptoPP::StringSink(mac)));
  std::string result;
  ::CryptoPP::StringSource(mac, true, new ::CryptoPP::StringSink(result));
  return result;
}

std::string CryptoPP::hmac_sha1(const std::string& key,
                                const std::string& message) {
  std::string mac;
  ::CryptoPP::HMAC<::CryptoPP::SHA1> hmac((byte*)key.c_str(), key.length());
  ::CryptoPP::StringSource(
      message, true,
      new ::CryptoPP::HashFilter(hmac, new ::CryptoPP::StringSink(mac)));
  std::string result;
  ::CryptoPP::StringSource(mac, true, new ::CryptoPP::StringSink(result));
  return result;
}

std::string CryptoPP::hex(const std::string& hash) {
  std::string result;
  ::CryptoPP::StringSource(
      hash, true,
      new ::CryptoPP::HexEncoder(new ::CryptoPP::StringSink(result), false));
  return result;
}

}  // namespace cloudstorage

#endif  // WITH_CRYPTOPP
