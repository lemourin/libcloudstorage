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

namespace cloudstorage {

Item::Item(std::string filename, std::string id, bool is_directory)
    : filename_(filename), id_(id), is_directory_(is_directory) {}

std::string Item::filename() const { return filename_; }

std::string Item::id() const { return id_; }

bool Item::is_directory() const { return is_directory_; }

}  // namespace cloudstorage
