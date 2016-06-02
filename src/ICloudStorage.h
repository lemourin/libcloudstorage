/*****************************************************************************
 * ICloudStorage.h : interface of ICloudStorage
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

#ifndef ICLOUDSTORAGE_H
#define ICLOUDSTORAGE_H

#include <jsoncpp/json/forwards.h>
#include <vector>
#include "ICloudProvider.h"

namespace cloudstorage {

class ICloudStorage {
 public:
  virtual ~ICloudStorage() = default;

  virtual std::vector<ICloudProvider::Pointer> providers() const = 0;
  virtual ICloudProvider::Pointer provider(const std::string& name) const = 0;
  virtual ICloudProvider::Pointer providerFromJson(
      const Json::Value& data) const = 0;
  virtual ICloudProvider::Pointer providerFromFile(
      const std::string& file) const = 0;
};

}  // namespace cloudstorage

#endif  // ICLOUDSTORAGE_H
