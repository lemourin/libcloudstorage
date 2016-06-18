/*****************************************************************************
 * CloudProvider.cpp : implementation of CloudProvider
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

#include "CloudProvider.h"

#include <jsoncpp/json/json.h>
#include <sstream>

#include "Item.h"
#include "Utility.h"

namespace cloudstorage {

namespace {
class Callback : public IAuth::ICallback {
 public:
  Callback(ICloudProvider::ICallback::Pointer callback,
           const ICloudProvider& provider)
      : callback_(std::move(callback)), provider_(provider) {}

  Status userConsentRequired(const IAuth&) const {
    if (callback_) {
      if (callback_->userConsentRequired(provider_) ==
          ICloudProvider::ICallback::Status::WaitForAuthorizationCode)
        return Status::WaitForAuthorizationCode;
    }
    return Status::None;
  }

 private:
  ICloudProvider::ICallback::Pointer callback_;
  const ICloudProvider& provider_;
};
}  // namespace

CloudProvider::CloudProvider(IAuth::Pointer auth) : auth_(std::move(auth)) {}

bool CloudProvider::initialize(const std::string& token,
                               ICallback::Pointer callback) {
  auth_callback_ =
      make_unique<cloudstorage::Callback>(std::move(callback), *this);

  std::lock_guard<std::mutex> lock(auth_mutex_);
  auth()->set_access_token(auth()->fromTokenString(token));
  return auth()->authorize(auth_callback_.get());
}

std::string CloudProvider::access_token() {
  std::lock_guard<std::mutex> lock(auth_mutex_);
  if (auth()->access_token() == nullptr)
    throw std::logic_error("Not authenticated yet.");
  return auth()->access_token()->token_;
}

IAuth* CloudProvider::auth() const { return auth_.get(); }

std::vector<IItem::Pointer> CloudProvider::listDirectory(const IItem& item) {
  return execute(&CloudProvider::executeListDirectory, item);
}

void CloudProvider::uploadFile(const IItem& directory,
                               const std::string& filename,
                               std::istream& stream) {
  execute(&CloudProvider::executeUploadFile, directory, filename, stream);
}

void CloudProvider::downloadFile(const IItem& item, std::ostream& stream) {
  execute(&CloudProvider::executeDownloadFile, item, stream);
}

std::string CloudProvider::authorizeLibraryUrl() const {
  return auth()->authorizeLibraryUrl();
}

std::string CloudProvider::token() {
  std::lock_guard<std::mutex> lock(auth_mutex_);
  if (auth()->access_token() == nullptr)
    throw std::logic_error("Not authenticated yet.");
  return auth()->access_token()->refresh_token_;
}

IItem::Pointer CloudProvider::rootDirectory() const {
  return make_unique<Item>("/", "root", true);
}

IItem::Pointer CloudProvider::getItem(std::vector<IItem::Pointer>&& items,
                                      const std::string& name) const {
  for (IItem::Pointer& i : items)
    if (i->filename() == name) return std::move(i);
  return nullptr;
}

IItem::Pointer CloudProvider::getItem(const std::string& path) {
  if (path.empty() || path.front() != '/') return nullptr;
  IItem::Pointer node = rootDirectory();
  std::stringstream stream(path.substr(1));
  std::string token;
  while (std::getline(stream, token, '/')) {
    if (!node || !node->is_directory()) return nullptr;
    node = getItem(listDirectory(*node), token);
  }
  return node;
}

}  // namespace cloudstorage
