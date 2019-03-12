/*****************************************************************************
 * CloudStorage.cpp
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

#include "CloudStorage.h"
#include "ICloudStorage.h"

#include <cstring>
#include <iostream>

using namespace cloudstorage;

cloud_storage* cloud_storage_create() {
  return reinterpret_cast<cloud_storage*>(
      ICloudStorage::create()->create().release());
}

void cloud_storage_release(cloud_storage* t) {
  delete reinterpret_cast<ICloudStorage*>(t);
}

cloud_string_list* cloud_storage_provider_list(cloud_storage* p) {
  auto cloud = reinterpret_cast<ICloudStorage*>(p);
  return reinterpret_cast<cloud_string_list*>(
      new std::vector<std::string>(cloud->providers()));
}

void cloud_string_list_release(cloud_string_list* lst) {
  delete reinterpret_cast<std::vector<std::string>*>(lst);
}

size_t cloud_string_list_length(cloud_string_list* lst) {
  return reinterpret_cast<std::vector<std::string>*>(lst)->size();
}

cloud_string* cloud_string_list_get(cloud_string_list* lst, size_t idx) {
  auto str = (reinterpret_cast<std::vector<std::string>*>(lst))->at(idx);
  return cloud_string_create(str.c_str());
}

cloud_string* cloud_string_create(const char* data) {
  auto length = strlen(data);
  auto storage = malloc(length + 1);
  memcpy(storage, data, length + 1);
  return reinterpret_cast<char*>(storage);
}

void cloud_string_release(cloud_string* str) {
  free(const_cast<void*>(reinterpret_cast<const void*>(str)));
}

cloud_provider* cloud_storage_provider(cloud_storage* d, cloud_string* name,
                                       cloud_provider_init_data* data) {
  auto init_data = reinterpret_cast<ICloudProvider::InitData*>(data);
  return reinterpret_cast<cloud_provider*>(
      reinterpret_cast<ICloudStorage*>(d)
          ->provider(name, std::move(*init_data))
          .release());
}
