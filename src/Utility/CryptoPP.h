/*****************************************************************************
 * CryptoPP.h :
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

#ifndef CRYPTOPP_H
#define CRYPTOPP_H

#include "ICrypto.h"

namespace cloudstorage {

class CryptoPP : public ICrypto {
 public:
  std::string sha256(const std::string& message);
  std::string hmac_sha256(const std::string& key, const std::string& message);
  std::string hex(const std::string& hash);
};

}  // namespace cloudstroage

#endif  // CRYPTOPP_H
