/*****************************************************************************
 * Item.cpp : implementation of Item
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

#include "Item.h"

#include "Utility/Utility.h"

namespace cloudstorage {

Item::Item(std::string filename, std::string id, FileType type)
    : filename_(filename),
      id_(id),
      url_(),
      thumbnail_url_(),
      type_(type),
      is_hidden_(false) {}

std::string Item::filename() const { return filename_; }

std::string Item::id() const { return id_; }

std::string Item::url() const { return url_; }

void Item::set_url(std::string url) { url_ = url; }

std::string Item::thumbnail_url() const { return thumbnail_url_; }

void Item::set_thumbnail_url(std::string url) { thumbnail_url_ = url; }

bool Item::is_hidden() const { return is_hidden_; }

void Item::set_hidden(bool e) { is_hidden_ = e; }

IItem::FileType Item::type() const { return type_; }

void Item::set_type(FileType t) { type_ = t; }

}  // namespace cloudstorage
