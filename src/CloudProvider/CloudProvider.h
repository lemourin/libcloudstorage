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

#include "Utility/HttpRequest.h"
#include "ICloudProvider.h"
#include "Utility/Auth.h"

namespace cloudstorage {

class CloudProvider : public ICloudProvider,
                      public std::enable_shared_from_this<CloudProvider> {
 public:
  CloudProvider(IAuth::Pointer);

  AuthorizeRequest::Pointer initialize(const std::string& token,
                                       ICallback::Pointer);

  std::string access_token() const;
  IAuth* auth() const;

  std::string authorizeLibraryUrl() const;
  std::string token() const;
  IItem::Pointer rootDirectory() const;

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

 private:
  friend class Request;
  friend class AuthorizeRequest;

  IAuth::Pointer auth_;
  ICloudProvider::ICallback::Pointer callback_;
  mutable std::mutex auth_mutex_;
  std::mutex currently_authorizing_mutex_;
  bool currently_authorizing_;
  bool current_authorization_successful_;
  std::condition_variable authorized_;
};

}  // namespace cloudstorage

#endif  //  CLOUDPROVIDER_H
