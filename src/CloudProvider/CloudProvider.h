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

#include <mutex>
#include <sstream>
#include <thread>

#include "ICloudProvider.h"
#include "Request/AuthorizeRequest.h"
#include "Utility/Auth.h"
#include "Utility/HttpRequest.h"

namespace cloudstorage {

class CloudProvider : public ICloudProvider,
                      public std::enable_shared_from_this<CloudProvider> {
 public:
  using Pointer = std::shared_ptr<CloudProvider>;

  enum class AuthorizationStatus { None, InProgress, Fail, Success };

  CloudProvider(IAuth::Pointer);

  void initialize(const std::string& token, ICallback::Pointer,
                  const Hints& hints);

  Hints hints() const;
  std::string access_token() const;
  IAuth* auth() const;

  std::string authorizeLibraryUrl() const;
  std::string token() const;
  IItem::Pointer rootDirectory() const;
  ICallback* callback() const;

  virtual AuthorizeRequest::Pointer authorizeAsync();

  ListDirectoryRequest::Pointer listDirectoryAsync(
      IItem::Pointer, IListDirectoryCallback::Pointer);
  GetItemRequest::Pointer getItemAsync(const std::string& absolute_path,
                                       GetItemCallback);
  DownloadFileRequest::Pointer downloadFileAsync(
      IItem::Pointer, IDownloadFileCallback::Pointer);
  UploadFileRequest::Pointer uploadFileAsync(IItem::Pointer, const std::string&,
                                             IUploadFileCallback::Pointer);
  GetItemDataRequest::Pointer getItemDataAsync(const std::string& id,
                                               GetItemDataCallback f);
  DownloadFileRequest::Pointer getThumbnailAsync(
      IItem::Pointer, IDownloadFileCallback::Pointer);
  DeleteItemRequest::Pointer deleteItemAsync(IItem::Pointer,
                                             DeleteItemCallback);
  CreateDirectoryRequest::Pointer createDirectoryAsync(IItem::Pointer parent,
                                                       const std::string& name,
                                                       CreateDirectoryCallback);
  MoveItemRequest::Pointer moveItemAsync(IItem::Pointer source,
                                         IItem::Pointer destination,
                                         MoveItemCallback);
  RenameItemRequest::Pointer renameItemAsync(IItem::Pointer item,
                                             const std::string&,
                                             RenameItemCallback);

  virtual HttpRequest::Pointer getItemDataRequest(
      const std::string& id, std::ostream& input_stream) const;
  virtual HttpRequest::Pointer listDirectoryRequest(
      const IItem&, const std::string& page_token,
      std::ostream& input_stream) const;
  virtual HttpRequest::Pointer uploadFileRequest(
      const IItem& directory, const std::string& filename,
      std::ostream& prefix_stream, std::ostream& suffix_stream) const;
  virtual HttpRequest::Pointer downloadFileRequest(
      const IItem&, std::ostream& input_stream) const;
  virtual HttpRequest::Pointer getThumbnailRequest(
      const IItem&, std::ostream& input_stream) const;
  virtual HttpRequest::Pointer deleteItemRequest(
      const IItem&, std::ostream& input_stream) const;
  virtual HttpRequest::Pointer createDirectoryRequest(const IItem&,
                                                      const std::string&,
                                                      std::ostream&) const;
  virtual HttpRequest::Pointer moveItemRequest(const IItem& source,
                                               const IItem& destination,
                                               std::ostream&) const;
  virtual HttpRequest::Pointer renameItemRequest(const IItem& item,
                                                 const std::string& name,
                                                 std::ostream&) const;

  virtual IItem::Pointer getItemDataResponse(std::istream& response) const;
  virtual std::vector<IItem::Pointer> listDirectoryResponse(
      std::istream& response, std::string& next_page_token) const;
  virtual IItem::Pointer createDirectoryResponse(std::istream& response) const;

  virtual void authorizeRequest(HttpRequest&) const;
  virtual bool reauthorize(int code) const;

  std::mutex& auth_mutex() const;
  std::mutex& current_authorization_mutex() const;
  std::condition_variable& authorized_condition() const;

  AuthorizationStatus authorization_status() const;
  void set_authorization_status(AuthorizationStatus);

  AuthorizeRequest::Pointer current_authorization() const;
  void set_current_authorization(AuthorizeRequest::Pointer);

 protected:
  void setWithHint(const Hints& hints, const std::string& name,
                   std::function<void(std::string)>) const;

 private:
  IAuth::Pointer auth_;
  ICloudProvider::ICallback::Pointer callback_;
  AuthorizeRequest::Pointer current_authorization_;
  AuthorizationStatus current_authorization_status_;
  mutable std::mutex auth_mutex_;
  mutable std::mutex current_authorization_mutex_;
  mutable std::mutex authorization_status_mutex_;
  mutable std::condition_variable authorized_;
};

}  // namespace cloudstorage

#endif  //  CLOUDPROVIDER_H
