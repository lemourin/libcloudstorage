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

#include <cstdint>
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

  CloudProvider(IAuth::Pointer);

  virtual void initialize(InitData&&);
  virtual void destroy();

  Hints hints() const override;
  std::string access_token() const;
  IAuth* auth() const;

  std::string authorizeLibraryUrl() const override;
  std::string token() const override;
  IItem::Pointer rootDirectory() const override;
  OperationSet supportedOperations() const override;
  ICrypto* crypto() const;
  IHttp* http() const;
  IHttpServerFactory* http_server() const;
  IThreadPool* thread_pool() const;
  IAuthCallback* auth_callback() const;

  virtual bool isSuccess(int code, const IHttpRequest::HeaderParameters&) const;

  virtual AuthorizeRequest::Pointer authorizeAsync();

  GetItemUrlRequest::Pointer getItemUrlAsync(IItem::Pointer,
                                             GetItemUrlCallback) override;
  ExchangeCodeRequest::Pointer exchangeCodeAsync(const std::string&,
                                                 ExchangeCodeCallback) override;
  ListDirectoryRequest::Pointer listDirectoryAsync(
      IItem::Pointer, IListDirectoryCallback::Pointer) override;
  GetItemRequest::Pointer getItemAsync(const std::string& absolute_path,
                                       GetItemCallback) override;
  DownloadFileRequest::Pointer downloadFileAsync(IItem::Pointer,
                                                 IDownloadFileCallback::Pointer,
                                                 Range) override;
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
  ListDirectoryPageRequest::Pointer listDirectoryPageAsync(
      IItem::Pointer, const std::string&, ListDirectoryPageCallback) override;
  ListDirectoryRequest::Pointer listDirectorySimpleAsync(
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
  GeneralDataRequest::Pointer getGeneralDataAsync(GeneralDataCallback) override;

  /**
   * Used by default implementation of getItemDataAsync.
   *
   * @param id
   * @param input_stream request body
   * @return http request
   */
  virtual IHttpRequest::Pointer getItemDataRequest(
      const std::string& id, std::ostream& input_stream) const;

  virtual IHttpRequest::Pointer getItemUrlRequest(
      const IItem&, std::ostream& input_stream) const;

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

  virtual IHttpRequest::Pointer getGeneralDataRequest(std::ostream&) const;

  /**
   * Used by default implementation of getItemDataAsync, should translate
   * reponse into IItem object.
   *
   * @param response
   * @return item object
   */
  virtual IItem::Pointer getItemDataResponse(std::istream& response) const;

  virtual std::string getItemUrlResponse(const IItem& item,
                                         const IHttpRequest::HeaderParameters&,
                                         std::istream& response) const;

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
  virtual IItem::List listDirectoryResponse(const IItem& directory,
                                            std::istream& response,
                                            std::string& next_page_token) const;

  virtual IItem::Pointer renameItemResponse(const IItem& old_item,
                                            const std::string& name,
                                            std::istream& response) const;

  virtual IItem::Pointer moveItemResponse(const IItem&, const IItem&,
                                          std::istream&) const;

  virtual IItem::Pointer uploadFileResponse(const IItem& parent,
                                            const std::string& filename,
                                            uint64_t size,
                                            std::istream& response) const;
  virtual GeneralData getGeneralDataResponse(std::istream& response) const;

  /**
   * Used by default implementation of createDirectoryAsync, should translate
   * response into new directory's item object.
   *
   * @param response
   *
   * @return item object
   */
  virtual IItem::Pointer createDirectoryResponse(const IItem& parent,
                                                 const std::string& name,
                                                 std::istream& response) const;

  /**
   * Should add access token to the request, by default sets Authorization
   * header parameter to "Bearer access_token()".
   *
   * @param request request to be authorized
   */
  virtual void authorizeRequest(IHttpRequest& request) const;

  /**
   * Returns whether we should try to authorize again after receiving response
   * with http code.
   *
   * @param code http code
   * @return whether to do authorization again or not
   */
  virtual bool reauthorize(int code,
                           const IHttpRequest::HeaderParameters&) const;

  virtual bool unpackCredentials(const std::string&);

  std::unique_lock<std::mutex> auth_lock() const;

  static std::string getPath(const std::string&);
  static std::string getFilename(const std::string& path);
  static Json::Value credentialsFromString(const std::string&);
  static std::string credentialsToString(const Json::Value& json);

 protected:
  void setWithHint(const Hints& hints, const std::string& name,
                   std::function<void(std::string)>) const;

 private:
  friend class AuthorizeRequest;
  template <class T>
  friend class Request;

  IAuth::Pointer auth_;
  IAuthCallback::Pointer callback_;
  ICrypto::Pointer crypto_;
  IHttp::Pointer http_;
  IHttpServerFactory::Pointer http_server_;
  IThreadPool::Pointer thread_pool_;
  AuthorizeRequest::Pointer current_authorization_;
  std::unordered_map<IGenericRequest*,
                     std::vector<AuthorizeRequest::AuthorizeCompleted>>
      auth_callbacks_;
  std::mutex current_authorization_mutex_;
  mutable std::mutex auth_mutex_;
};

}  // namespace cloudstorage

#endif  //  CLOUDPROVIDER_H
