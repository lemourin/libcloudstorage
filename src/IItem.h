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

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#ifdef _MSC_VER

#ifdef CLOUDSTORAGE_LIBRARY
#define CLOUDSTORAGE_API __declspec(dllexport)
#else
#define CLOUDSTORAGE_API __declspec(dllimport)
#endif

#else
#define CLOUDSTORAGE_API
#endif  // _MSC_VER

namespace cloudstorage {

class CLOUDSTORAGE_API IItem {
 public:
  using Pointer = std::shared_ptr<IItem>;
  using TimeStamp = std::chrono::system_clock::time_point;
  using List = std::vector<IItem::Pointer>;

  static constexpr size_t UnknownSize =
      (std::numeric_limits<std::size_t>::max)();
  static const TimeStamp UnknownTimeStamp;

  enum class FileType { Directory, Video, Audio, Image, Unknown };

  virtual ~IItem() = default;

  virtual TimeStamp timestamp() const = 0;
  virtual std::string filename() const = 0;
  virtual std::string extension() const = 0;
  virtual std::string id() const = 0;
  virtual size_t size() const = 0;

  virtual bool is_hidden() const = 0;
  virtual FileType type() const = 0;

  virtual std::string toString() const = 0;
  static IItem::Pointer fromString(const std::string&);
};

}  // namespace cloudstorage

#endif  // IITEM_H
