/*****************************************************************************
 * Item.h : interface for Item
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

#ifndef ITEM_H
#define ITEM_H

#include <mutex>
#include <string>
#include <vector>

#include "IItem.h"

namespace cloudstorage {

class Item : public IItem {
 public:
  using Pointer = std::shared_ptr<Item>;

  Item(std::string filename, std::string id, size_t size, TimeStamp, FileType);

  std::string filename() const override;
  void set_filename(std::string);

  std::string extension() const override;
  std::string id() const override;
  TimeStamp timestamp() const override;

  size_t size() const override;
  void set_size(size_t);

  std::string url() const;
  void set_url(std::string);

  std::string thumbnail_url() const;
  void set_thumbnail_url(std::string);

  bool is_hidden() const override;
  void set_hidden(bool);

  FileType type() const override;
  void set_type(FileType);

  const std::vector<std::string>& parents() const;
  void set_parents(const std::vector<std::string>&);

  std::string mime_type() const;
  void set_mime_type(const std::string&);

  static FileType fromMimeType(const std::string& mime_type);
  static FileType fromExtension(const std::string& filename);

 private:
  mutable std::mutex mutex_;
  std::string filename_;
  std::string id_;
  std::string url_;
  size_t size_;
  TimeStamp timestamp_;
  std::string thumbnail_url_;
  FileType type_;
  bool is_hidden_;
  std::string mime_type_;
  std::vector<std::string> parents_;
};

}  // namespace cloudstorage

#endif  // ITEM_H
