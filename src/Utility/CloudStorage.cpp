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

CloudStorage::CloudStorage()
    : providers_({std::make_shared<GoogleDrive>(), std::make_shared<OneDrive>(),
                  std::make_shared<Dropbox>(), std::make_shared<AmazonDrive>(),
                  std::make_shared<Box>(), std::make_shared<YouTube>(),
                  std::make_shared<YandexDisk>(), std::make_shared<OwnCloud>(),
                  std::make_shared<AmazonS3>()}) {
#ifdef WITH_MEGA
  providers_.emplace_back(std::make_shared<MegaNz>());
#endif
}

std::vector<ICloudProvider::Pointer> CloudStorage::providers() const {
  return providers_;
}

ICloudProvider::Pointer CloudStorage::provider(const std::string& name) const {
  for (ICloudProvider::Pointer p : providers_)
    if (p->name() == name) return p;
  return nullptr;
}

ICloudStorage::Pointer ICloudStorage::create() {
  return make_unique<CloudStorage>();
}

}  // namespace cloudstorage
