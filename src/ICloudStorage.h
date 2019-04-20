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

#include <vector>

#include "ICloudProvider.h"

namespace cloudstorage {

/**
 * Contains available CloudStorage providers.
 */
class CLOUDSTORAGE_API ICloudStorage {
 public:
  using Pointer = std::unique_ptr<ICloudStorage>;

  virtual ~ICloudStorage() = default;

  /**
   * Retrieves list of available cloud providers.
   *
   * @return cloud providers
   */
  virtual std::vector<std::string> providers() const = 0;

  /**
   * Gets provider by name.
   *
   * @param name
   * @return cloud provider
   */
  virtual ICloudProvider::Pointer provider(
      const std::string& name, ICloudProvider::InitData&&) const = 0;

  static ICloudStorage::Pointer create();
  static void initialize(void*);
};

}  // namespace cloudstorage

#endif  // ICLOUDSTORAGE_H
