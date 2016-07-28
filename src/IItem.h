/*****************************************************************************
 * IItem.h : interface for IItem
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

#ifndef IITEM_H
#define IITEM_H

#include <memory>
#include <string>

namespace cloudstorage {

class IItem {
 public:
  using Pointer = std::shared_ptr<IItem>;

  enum class FileType { Directory, Video, Audio, Image, Unknown };

  virtual ~IItem() = default;

  virtual std::string filename() const = 0;
  virtual std::string id() const = 0;

  /**
   * This url is valid if IItem instance was received by
   * ICloudProvider::getItemDataAsync; it's because some cloud providers require
   * to generate temporary urls per item; ICloudProvider::getItemDataAsync cares
   * about that, ICloudProvider::listDirectoryAsync does not.
   *
   * @return url
   */
  virtual std::string url() const = 0;

  virtual bool is_hidden() const = 0;
  virtual FileType type() const = 0;
};

}  // namespace cloudstorage

#endif  // IITEM_H
