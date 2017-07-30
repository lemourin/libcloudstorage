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

namespace cloudstorage {

class CloudProvider : public ICloudProvider,
                      public std::enable_shared_from_this<CloudProvider> {
 public:
  using Pointer = std::shared_ptr<CloudProvider>;

  enum class AuthorizationStatus { Done, InProgress };

  CloudProvider(IAuth::Pointer);

  void initialize(InitData&&) override;

  Hints hints() const override;
  std::string access_token() const;
  IAuth* auth() const;

  std::string authorizeLibraryUrl() const override;
  std::string token() const override;
  IItem::Pointer rootDirectory() const override;
  ICallback* callback() const;
  ICrypto* crypto() const;
  IHttp* http() const;
  IThumbnailer* thumbnailer() const;
  IHttpServerFactory* http_server() const;

  virtual AuthorizeRequest::Pointer authorizeAsync();

  ExchangeCodeRequest::Pointer exchangeCodeAsync(const std::string&,
                                                 ExchangeCodeCallback) override;
  ListDirectoryRequest::Pointer listDirectoryAsync(
      IItem::Pointer, IListDirectoryCallback::Pointer) override;
  GetItemRequest::Pointer getItemAsync(const std::string& absolute_path,
                                       GetItemCallback) override;
  DownloadFileRequest::Pointer downloadFileAsync(
      IItem::Pointer, IDownloadFileCallback::Pointer) override;
  UploadFileRequest::Pointer uploadFileAsync(
      IItem::Pointer, const std::string&,
      IUploadFileCallback::Pointer) override;
  GetItemDataRequest::Pointer getItemDataAsync(const std::string& id,
                                               GetItemDataCallback f) override;
  DownloadFileRequest::Pointer getThumbnailAsync(
      IItem::Pointer, IDownloadFileCallback::Pointer) override;
  DeleteItemRequest::Pointer deleteItemAsync(IItem::Pointer,
                                             DeleteItemCallback) override;
  CreateDirectoryRequest::Pointer createDirectoryAsync(
      IItem::Pointer parent, const std::string& name,
      CreateDirectoryCallback) override;
  MoveItemRequest::Pointer moveItemAsync(IItem::Pointer source,
                                         IItem::Pointer destination,
                                         MoveItemCallback) override;
  RenameItemRequest::Pointer renameItemAsync(IItem::Pointer item,
                                             const std::string&,
                                             RenameItemCallback) override;
  ListDirectoryRequest::Pointer listDirectoryAsync(
      IItem::Pointer item, ListDirectoryCallback callback) override;
  DownloadFileRequest::Pointer downloadFileAsync(IItem::Pointer item,
                                                 const std::string& filename,
                                                 DownloadFileCallback) override;
  DownloadFileRequest::Pointer getThumbnailAsync(IItem::Pointer item,
                                                 const std::string& filename,
                                                 GetThumbnailCallback) override;
  UploadFileRequest::Pointer uploadFileAsync(IItem::Pointer parent,
                                             const std::string& path,
                                             const std::string& filename,
                                             UploadFileCallback) override;

  /**
   * Used by default implementation of getItemDataAsync.
   *
   * @param id
   * @param input_stream request body
   * @return http request
   */
  virtual IHttpRequest::Pointer getItemDataRequest(
      const std::string& id, std::ostream& input_stream) const;

  /**
   * Used by default implementation of listDirectoryAsync.
   *
   * @param page_token page token
   * @param input_stream request body
   * @return http request
   */
  virtual IHttpRequest::Pointer listDirectoryRequest(
      const IItem&, const std::string& page_token,
      std::ostream& input_stream) const;

  /**
   * Used by default implementation of uploadFileAsync.
   *
   * @param directory
   * @param filename
   * @param prefix_stream what should be sent before the file in request's body
   * @param suffix_stream what should be sent after the file in request's body
   * @return http request
   */
  virtual IHttpRequest::Pointer uploadFileRequest(
      const IItem& directory, const std::string& filename,
      std::ostream& prefix_stream, std::ostream& suffix_stream) const;

  /**
   * Used by default implementation of downloadFileAsync.
   *
   * @param input_stream request body
   * @return http request
   */
  virtual IHttpRequest::Pointer downloadFileRequest(
      const IItem&, std::ostream& input_stream) const;

  /**
   * Used by default implementation of getThumbnailAsync; tries to download
   * thumbnail from Item::thumnail_url.
   *
   * @param item
   * @param input_stream request body
   * @return http request
   */
  virtual IHttpRequest::Pointer getThumbnailRequest(
      const IItem& item, std::ostream& input_stream) const;

  /**
   * Used by default implementation of deleteItemAsync.
   *
   * @param input_stream request body
   * @return http request
   */
  virtual IHttpRequest::Pointer deleteItemRequest(
      const IItem&, std::ostream& input_stream) const;

  /**
   * Used by default implementation of createDirectoryAsync.
   *
   * @return http request
   */
  virtual IHttpRequest::Pointer createDirectoryRequest(const IItem&,
                                                       const std::string&,
                                                       std::ostream&) const;

  /**
   * Used by default implementation of moveItemAsync.
   *
   * @param source
   * @param destination
   * @return http request
   */
  virtual IHttpRequest::Pointer moveItemRequest(const IItem& source,
                                                const IItem& destination,
                                                std::ostream&) const;

  /**
   * Used by default implementation of renameItemAsync.
   *
   * @param item
   * @param name
   * @return http request
   */
  virtual IHttpRequest::Pointer renameItemRequest(const IItem& item,
                                                  const std::string& name,
                                                  std::ostream&) const;

  /**
   * Used by default implementation of getItemDataAsync, should translate
   * reponse into IItem object.
   *
   * @param response
   * @return item object
   */
  virtual IItem::Pointer getItemDataResponse(std::istream& response) const;

  /**
   * Used by default implementation of listDirectoryAsync, should extract items
   * from response.
   *
   * @param response
   *
   * @param next_page_token should be set to string describing the next page or
   * to empty string if there is no next page
   *
   * @return item set
   */
  virtual std::vector<IItem::Pointer> listDirectoryResponse(
      std::istream& response, std::string& next_page_token) const;

  /**
   * Used by default implementation of createDirectoryAsync, should translate
   * response into new directory's item object.
   *
   * @param response
   *
   * @return item object
   */
  virtual IItem::Pointer createDirectoryResponse(std::istream& response) const;

  /**
   * Should add access token to the request, by default sets Authorization
   * header parameter to "Bearer access_token()".
   *
   * @param request request to be authorized
   */
  virtual void authorizeRequest(IHttpRequest& request) const;

  /**
   * Returns whether we should try to authorize again after receiving response
   * with http code / curl error.
   *
   * @param code http code / curl error
   * @return whether to do authorization again or not
   */
  virtual bool reauthorize(int code) const;

  std::mutex& auth_mutex() const;
  std::mutex& current_authorization_mutex() const;
  std::condition_variable& authorized_condition() const;

  AuthorizationStatus authorization_status() const;
  void set_authorization_status(AuthorizationStatus);

  EitherError<void> authorization_result() const;
  void set_authorization_result(EitherError<void>);

  AuthorizeRequest::Pointer current_authorization() const;
  void set_current_authorization(AuthorizeRequest::Pointer);

  int authorization_request_count() const;
  void set_authorization_request_count(int);

  static std::string getPath(const std::string&);
  static std::string getFilename(const std::string& path);
  static std::pair<std::string, std::string> credentialsFromString(
      const std::string&);

 protected:
  void setWithHint(const Hints& hints, const std::string& name,
                   std::function<void(std::string)>) const;

 private:
  IAuth::Pointer auth_;
  ICloudProvider::ICallback::Pointer callback_;
  ICrypto::Pointer crypto_;
  IHttp::Pointer http_;
  IThumbnailer::Pointer thumbnailer_;
  IHttpServerFactory::Pointer http_server_;
  AuthorizeRequest::Pointer current_authorization_;
  AuthorizationStatus current_authorization_status_;
  EitherError<void> authorization_result_;
  int authorization_request_count_;
  mutable std::mutex auth_mutex_;
  mutable std::mutex current_authorization_mutex_;
  mutable std::mutex authorization_status_mutex_;
  mutable std::condition_variable authorized_;
};

}  // namespace cloudstorage

#endif  //  CLOUDPROVIDER_H
