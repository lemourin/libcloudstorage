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

#include <jsoncpp/json/json.h>
#include <fstream>

#include "Dropbox.h"
#include "GoogleDrive.h"
#include "OneDrive.h"
#include "Utility.h"

namespace cloudstorage {

std::vector<ICloudProvider::Pointer> CloudStorage::providers() const {
  std::vector<ICloudProvider::Pointer> result;
  result.push_back(make_unique<GoogleDrive>());
  result.push_back(make_unique<OneDrive>());
  result.push_back(make_unique<Dropbox>());
  return result;
}

ICloudProvider::Pointer CloudStorage::provider(const std::string& name) const {
  for (ICloudProvider::Pointer& p : providers())
    if (p->name() == name) return std::move(p);
  return nullptr;
}

ICloudProvider::Pointer CloudStorage::providerFromJson(
    const Json::Value& data) const {
  ICloudProvider::Pointer p = provider(data["backend"].asString());
  CloudProvider* cloudprovider = static_cast<CloudProvider*>(p.get());
  if (!cloudprovider) return nullptr;
  if (data.isMember("token"))
    cloudprovider->auth()->set_token_data(data["token"]);
  return p;
}

ICloudProvider::Pointer CloudStorage::providerFromFile(
    const std::string& file) const {
  std::fstream stream(file);
  if (!stream.is_open()) return nullptr;
  Json::Value data;
  stream >> data;
  return providerFromJson(data);
}

}  // namespace cloudstorage
