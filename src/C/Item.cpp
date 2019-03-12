/*****************************************************************************
 * Item.cpp
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

#include "Item.h"
#include "CloudStorage.h"

#include <cstring>
#include "IItem.h"

using namespace cloudstorage;

void cloud_item_release(cloud_item *item) {
  delete reinterpret_cast<IItem::Pointer *>(item);
}

cloud_string *cloud_item_filename(const cloud_item *i) {
  return cloud_string_create(
      (*reinterpret_cast<const IItem::Pointer *>(i))->filename().c_str());
}

cloud_string *cloud_item_id(const cloud_item *i) {
  return cloud_string_create(
      (*reinterpret_cast<const IItem::Pointer *>(i))->id().c_str());
}

cloud_string *cloud_item_extension(const struct cloud_item *i) {
  return cloud_string_create(
      (*reinterpret_cast<const IItem::Pointer *>(i))->extension().c_str());
}

cloud_string *cloud_item_to_string(const cloud_item *i) {
  return cloud_string_create(
      (*reinterpret_cast<const IItem::Pointer *>(i))->toString().c_str());
}

time_t cloud_item_timestamp(const cloud_item *i) {
  return std::chrono::system_clock::to_time_t(
      (*reinterpret_cast<const IItem::Pointer *>(i))->timestamp());
}

size_t cloud_item_size(const cloud_item *i) {
  return (*reinterpret_cast<const IItem::Pointer *>(i))->size();
}

enum cloud_item_type cloud_item_type(const cloud_item *i) {
  return static_cast<enum cloud_item_type>(
      (*reinterpret_cast<const IItem::Pointer *>(i))->type());
}

int cloud_item_is_hidden(const struct cloud_item *i) {
  return (*reinterpret_cast<const IItem::Pointer *>(i))->is_hidden();
}

cloud_item *cloud_item_from_string(cloud_string *str) {
  return reinterpret_cast<cloud_item *>(
      new IItem::Pointer(IItem::fromString(str)));
}
