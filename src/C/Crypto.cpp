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

#include "ICrypto.h"

using namespace cloudstorage;

cloud_crypto *cloud_crypto_create() {
  return reinterpret_cast<cloud_crypto *>(
      new std::unique_ptr<ICrypto>(ICrypto::create()));
}

void cloud_crypto_release(cloud_crypto *d) {
  delete reinterpret_cast<std::unique_ptr<ICrypto> *>(d);
}
