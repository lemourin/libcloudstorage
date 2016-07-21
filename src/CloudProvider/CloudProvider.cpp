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

#include <json/json.h>
#include <sstream>

#include "Utility/Item.h"
#include "Utility/Utility.h"

#include "Request/DeleteItemRequest.h"
#include "Request/DownloadFileRequest.h"
#include "Request/GetItemDataRequest.h"
#include "Request/GetItemRequest.h"
#include "Request/ListDirectoryRequest.h"
#include "Request/UploadFileRequest.h"

using namespace std::placeholders;

namespace cloudstorage {

CloudProvider::CloudProvider(IAuth::Pointer auth)
    : auth_(std::move(auth)),
      current_authorization_status_(AuthorizationStatus::None) {}

void CloudProvider::initialize(const std::string& token,
                               ICallback::Pointer callback,
                               const Hints& hints) {
  std::lock_guard<std::mutex> lock(auth_mutex_);
  callback_ = std::move(callback);

  auto t = auth()->fromTokenString(token);
  setWithHint(hints, "access_token", [&t](std::string v) { t->token_ = v; });
  auth()->set_access_token(std::move(t));

  setWithHint(hints, "client_id",
              [this](std::string v) { auth()->set_client_id(v); });
  setWithHint(hints, "client_secret",
              [this](std::string v) { auth()->set_client_secret(v); });
}

ICloudProvider::Hints CloudProvider::hints() const {
  return {{"access_token", access_token()}};
}

std::string CloudProvider::access_token() const {
  std::lock_guard<std::mutex> lock(auth_mutex_);
  if (auth()->access_token() == nullptr) return "";
  return auth()->access_token()->token_;
}

IAuth* CloudProvider::auth() const { return auth_.get(); }

std::string CloudProvider::authorizeLibraryUrl() const {
  return auth()->authorizeLibraryUrl();
}

std::string CloudProvider::token() const {
  std::lock_guard<std::mutex> lock(auth_mutex_);
  if (auth()->access_token() == nullptr) return "";
  return auth()->access_token()->refresh_token_;
}

IItem::Pointer CloudProvider::rootDirectory() const {
  return make_unique<Item>("/", "root", IItem::FileType::Directory);
}

ICloudProvider::ICallback* CloudProvider::callback() const {
  return callback_.get();
}

ICloudProvider::ListDirectoryRequest::Pointer CloudProvider::listDirectoryAsync(
    IItem::Pointer item, IListDirectoryCallback::Pointer callback) {
  return make_unique<cloudstorage::ListDirectoryRequest>(
      shared_from_this(), std::move(item), std::move(callback));
}

ICloudProvider::GetItemRequest::Pointer CloudProvider::getItemAsync(
    const std::string& absolute_path, GetItemCallback callback) {
  return make_unique<cloudstorage::GetItemRequest>(shared_from_this(),
                                                   absolute_path, callback);
}

ICloudProvider::DownloadFileRequest::Pointer CloudProvider::downloadFileAsync(
    IItem::Pointer file, IDownloadFileCallback::Pointer callback) {
  return make_unique<cloudstorage::DownloadFileRequest>(
      shared_from_this(), std::move(file), std::move(callback),
      std::bind(&CloudProvider::downloadFileRequest, this, _1, _2));
}

ICloudProvider::UploadFileRequest::Pointer CloudProvider::uploadFileAsync(
    IItem::Pointer directory, const std::string& filename,
    IUploadFileCallback::Pointer callback) {
  return make_unique<cloudstorage::UploadFileRequest>(
      shared_from_this(), std::move(directory), filename, std::move(callback));
}

ICloudProvider::GetItemDataRequest::Pointer CloudProvider::getItemDataAsync(
    const std::string& id, GetItemDataCallback f) {
  return make_unique<cloudstorage::GetItemDataRequest>(shared_from_this(), id,
                                                       f);
}

void CloudProvider::authorizeRequest(HttpRequest& r) const {
  r.setHeaderParameter("Authorization", "Bearer " + access_token());
}

bool CloudProvider::reauthorize(int code) const {
  return HttpRequest::isAuthorizationError(code);
}

AuthorizeRequest::Pointer CloudProvider::authorizeAsync() {
  return make_unique<AuthorizeRequest>(shared_from_this());
}

std::mutex& CloudProvider::auth_mutex() const { return auth_mutex_; }

std::mutex& CloudProvider::current_authorization_mutex() const {
  return current_authorization_mutex_;
}

std::condition_variable& CloudProvider::authorized_condition() const {
  return authorized_;
}

CloudProvider::AuthorizationStatus CloudProvider::authorization_status() const {
  std::unique_lock<std::mutex> lock(authorization_status_mutex_);
  return current_authorization_status_;
}

void CloudProvider::set_authorization_status(
    CloudProvider::AuthorizationStatus status) {
  std::unique_lock<std::mutex> lock(authorization_status_mutex_);
  current_authorization_status_ = status;
}

AuthorizeRequest::Pointer CloudProvider::current_authorization() const {
  return current_authorization_;
}

void CloudProvider::set_current_authorization(AuthorizeRequest::Pointer r) {
  current_authorization_ = r;
}

void CloudProvider::setWithHint(const ICloudProvider::Hints& hints,
                                const std::string& name,
                                std::function<void(std::string)> f) const {
  auto it = hints.find(name);
  if (it != hints.end()) f(it->second);
}

ICloudProvider::DownloadFileRequest::Pointer CloudProvider::getThumbnailAsync(
    IItem::Pointer item, IDownloadFileCallback::Pointer callback) {
  return make_unique<cloudstorage::DownloadFileRequest>(
      shared_from_this(), item, std::move(callback),
      std::bind(&CloudProvider::getThumbnailRequest, this, _1, _2));
}

ICloudProvider::DeleteItemRequest::Pointer CloudProvider::deleteItemAsync(
    IItem::Pointer item, DeleteItemCallback callback) {
  return make_unique<cloudstorage::DeleteItemRequest>(shared_from_this(), item,
                                                      callback);
}

HttpRequest::Pointer CloudProvider::getItemDataRequest(const std::string&,
                                                       std::ostream&) const {
  return nullptr;
}

HttpRequest::Pointer CloudProvider::listDirectoryRequest(const IItem&,
                                                         const std::string&,
                                                         std::ostream&) const {
  return nullptr;
}

HttpRequest::Pointer CloudProvider::uploadFileRequest(const IItem&,
                                                      const std::string&,
                                                      std::istream&,
                                                      std::ostream&) const {
  return nullptr;
}

HttpRequest::Pointer CloudProvider::downloadFileRequest(const IItem&,
                                                        std::ostream&) const {
  return nullptr;
}

HttpRequest::Pointer CloudProvider::getThumbnailRequest(const IItem& i,
                                                        std::ostream&) const {
  const Item& item = static_cast<const Item&>(i);
  if (item.thumbnail_url().empty()) return nullptr;
  return make_unique<HttpRequest>(item.thumbnail_url(), HttpRequest::Type::GET);
}

HttpRequest::Pointer CloudProvider::deleteItemRequest(const IItem&,
                                                      std::ostream&) const {
  return nullptr;
}

IItem::Pointer CloudProvider::getItemDataResponse(std::istream&) const {
  return nullptr;
}

std::vector<IItem::Pointer> CloudProvider::listDirectoryResponse(
    std::istream&, std::string&) const {
  return {};
}

}  // namespace cloudstorage
