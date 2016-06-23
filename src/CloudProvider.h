/*****************************************************************************
 * CloudProvider.h : prototypes of CloudProvider
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

#ifndef CLOUDPROVIDER_H
#define CLOUDPROVIDER_H

#include <future>
#include <mutex>
#include <sstream>
#include <thread>

#include "Auth.h"
#include "HttpRequest.h"
#include "ICloudProvider.h"
#include "Request.h"

namespace cloudstorage {

class CloudProvider : public ICloudProvider,
                      public std::enable_shared_from_this<CloudProvider> {
 public:
  CloudProvider(IAuth::Pointer);

  bool initialize(const std::string& token, ICallback::Pointer);

  std::string access_token();
  IAuth* auth() const;

  std::vector<IItem::Pointer> listDirectory(const IItem&) final;
  void uploadFile(const IItem& directory, const std::string& filename,
                  std::istream&) final;
  void downloadFile(const IItem&, std::ostream&) final;
  IItem::Pointer getItem(const std::string&) final;

  std::string authorizeLibraryUrl() const;
  std::string token();
  IItem::Pointer rootDirectory() const;

  template <typename ReturnType, typename... FArgs, typename... Args>
  ReturnType execute(ReturnType (CloudProvider::*f)(FArgs...), Args&&... args) {
    try {
      if (!isAuthorized()) authorize();
      return (this->*f)(std::forward<Args>(args)...);
    } catch (const AuthorizationException&) {
      if (!authorize()) throw std::logic_error("Authorization failed.");
      return (this->*f)(std::forward<Args>(args)...);
    }
  }

  ListDirectoryRequest::Pointer listDirectoryAsync(
      IItem::Pointer, ListDirectoryRequest::ICallback::Pointer);
  GetItemRequest::Pointer getItemAsync(const std::string& absolute_path,
                                       std::function<void(IItem::Pointer)>);
  DownloadFileRequest::Pointer downloadFileAsync(
      IItem::Pointer, DownloadFileRequest::ICallback::Pointer);
  UploadFileRequest::Pointer uploadFileAsync(
      IItem::Pointer, const std::string&,
      UploadFileRequest::ICallback::Pointer);

  virtual HttpRequest::Pointer listDirectoryRequest(
      const IItem&, std::ostream& input_stream) const = 0;
  virtual HttpRequest::Pointer uploadFileRequest(
      const IItem& directory, const std::string& filename, std::istream& stream,
      std::ostream& input_stream) const = 0;
  virtual HttpRequest::Pointer downloadFileRequest(
      const IItem&, std::ostream& input_stream) const = 0;

  virtual std::vector<IItem::Pointer> listDirectoryResponse(
      std::istream& response, HttpRequest::Pointer& next_page_request,
      std::ostream& next_page_request_input) const = 0;

  virtual void authorizeRequest(HttpRequest&);

  void waitForAuthorized();
  bool isAuthorized();
  bool authorize();

 protected:
  virtual std::vector<IItem::Pointer> executeListDirectory(const IItem&) = 0;
  virtual void executeUploadFile(const IItem& directory,
                                 const std::string& filename,
                                 std::istream&) = 0;
  virtual void executeDownloadFile(const IItem&, std::ostream&) = 0;

 private:
  IItem::Pointer getItem(std::vector<IItem::Pointer>&& items,
                         const std::string& filename) const;

  IAuth::Pointer auth_;
  ICloudProvider::ICallback::Pointer callback_;
  std::mutex auth_mutex_;
  std::condition_variable authorized_;
};

}  // namespace cloudstorage

#endif  //  CLOUDPROVIDER_H
