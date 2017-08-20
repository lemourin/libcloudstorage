/*****************************************************************************
 * CloudStorage.cpp : implementation of CloudStorage
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

#include "CloudStorage.h"

#include "CloudProvider/AmazonDrive.h"
#include "CloudProvider/AmazonS3.h"
#include "CloudProvider/Box.h"
#include "CloudProvider/Dropbox.h"
#include "CloudProvider/GoogleDrive.h"
#include "CloudProvider/OneDrive.h"
#include "CloudProvider/OwnCloud.h"
#include "CloudProvider/YandexDisk.h"
#include "CloudProvider/YouTube.h"

#include "Utility/Utility.h"

#ifdef WITH_MEGA
#include "CloudProvider/MegaNz.h"
#endif

namespace cloudstorage {

CloudStorage::CloudStorage() {
  add([]() { return std::make_shared<GoogleDrive>(); });
  add([]() { return std::make_shared<OneDrive>(); });
  add([]() { return std::make_shared<Dropbox>(); });
  add([]() { return std::make_shared<AmazonDrive>(); });
  add([]() { return std::make_shared<Box>(); });
  add([]() { return std::make_shared<YouTube>(); });
  add([]() { return std::make_shared<YandexDisk>(); });
  add([]() { return std::make_shared<OwnCloud>(); });
  add([]() { return std::make_shared<AmazonS3>(); });
#ifdef WITH_MEGA
  add([]() { return std::make_shared<MegaNz>(); });
#endif
}

std::vector<std::string> CloudStorage::providers() const {
  std::vector<std::string> result;
  for (auto r : providers_) result.push_back(r.first);
  return result;
}

ICloudProvider::Pointer CloudStorage::provider(
    const std::string& name, ICloudProvider::InitData&& init_data) const {
  auto it = providers_.find(name);
  if (it == std::end(providers_)) return nullptr;
  auto ret = it->second();
  ret->initialize(std::move(init_data));
  return ret;
}

ICloudStorage::Pointer ICloudStorage::create() {
  return util::make_unique<CloudStorage>();
}

void CloudStorage::add(CloudProviderFactory f) { providers_[f()->name()] = f; }

}  // namespace cloudstorage
