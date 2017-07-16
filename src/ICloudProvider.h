/*****************************************************************************
 * ICloudProvider.h : prototypes of ICloudProvider
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

#ifndef ICLOUDPROVIDER_H
#define ICLOUDPROVIDER_H

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "ICrypto.h"
#include "IHttp.h"
#include "IHttpServer.h"
#include "IItem.h"
#include "IRequest.h"
#include "IThumbnailer.h"

namespace cloudstorage {

class ICloudProvider {
 public:
  using Pointer = std::shared_ptr<ICloudProvider>;
  using Hints = std::unordered_map<std::string, std::string>;

  using ListDirectoryRequest = IRequest<std::vector<IItem::Pointer>>;
  using GetItemRequest = IRequest<IItem::Pointer>;
  using DownloadFileRequest = IRequest<void>;
  using UploadFileRequest = IRequest<void>;
  using GetItemDataRequest = IRequest<IItem::Pointer>;
  using DeleteItemRequest = IRequest<bool>;
  using CreateDirectoryRequest = IRequest<IItem::Pointer>;
  using MoveItemRequest = IRequest<bool>;
  using RenameItemRequest = IRequest<bool>;

  class ICallback {
   public:
    using Pointer = std::shared_ptr<ICallback>;

    enum class Status { WaitForAuthorizationCode, None };

    virtual ~ICallback() = default;

    /**
     * Determines whether library should try to obtain authorization code or
     * not.
     *
     * @return Status::WaitForAuthorizationCode if the user wants to obtain the
     * authorization code, Status::None otherwise
     */
    virtual Status userConsentRequired(const ICloudProvider&) = 0;

    /**
     * Called when user gave his consent to our library.
     */
    virtual void accepted(const ICloudProvider&) = 0;

    /**
     * Called when user declined consent to our library.
     */
    virtual void declined(const ICloudProvider&) = 0;

    /**
     * Called when an error occurred.
     *
     * @param description error's description
     */
    virtual void error(const ICloudProvider&,
                       const std::string& description) = 0;
  };

  /**
   * Struct which provides initialization data for the cloud provider.
   */
  struct InitData {
    /**
     * Token retrieved by some previous run with ICloudProvider::token or any
     * string, library will detect whether it's valid and ask for user consent
     * if it isn't.
     */
    std::string token_;

    /**
     * Callback which will manage future authorization process.
     */
    ICallback::Pointer callback_;

    /**
     * Provides hashing methods which may be used by the cloud provider.
     */
    ICrypto::Pointer crypto_engine_;

    /**
     * Provides methods which are used for http communication.
     */
    IHttp::Pointer http_engine_;

    /**
     * Provides thumbnails when cloud provider doesn't have its own.
     */
    IThumbnailer::Pointer thumbnailer_;

    /**
     * Provides interface for creating http server.
     */
    IHttpServerFactory::Pointer http_server_;

    /**
    * Various hints which can be retrieved by some previous run with
    * ICloudProvider::hints; providing them may speed up the authorization
    * process; may contain the following:
    *  - client_id
    *  - client_secret
    *  - redirect_uri_port
    *  - state
    *  - access_token
    *  - daemon_port (used by mega.nz url provider)
    *  - metadata_url, content_url (amazon drive's endpoints)
    *  - youtube_dl_url (url to youtube-dl-server)
    *  - temporary_directory (used by mega.nz, has to use native path separators
    *    i.e. \ for windows and / for others; has to end with a separator)
    *  - login_page (html code of login page to be displayed when cloud provider
    *    doesn't use oauth; check for DEFAULT_LOGIN_PAGE to see what is the
    *    expected layout of the page)
    *  - success_page (html code of page to be displayed when library was
    *    authorized successfully)
    *  - error_page (html code of page to be displayed when library
    *    authorization failed)
    */
    Hints hints_;
  };

  virtual ~ICloudProvider() = default;

  /**
   * Initializes the cloud provider, doesn't do any authorization just yet. The
   * actual authorization will be done the first time it's actually needed.
   *
   * @param init_data initialization data
   */
  virtual void initialize(InitData&& init_data) = 0;

  /**
   * Token which should be saved and reused as a parameter to
   * ICloudProvider::initialize. Usually it's oauth2 refresh token.
   *
   * @return token
   */
  virtual std::string token() const = 0;

  /**
   * Returns hints which can be reused as a parameter to
   * ICloudProvider::initialize.
   *
   * @return hints
   */
  virtual Hints hints() const = 0;

  /**
   * Returns the name of cloud provider, used to instantinate it with
   * ICloudStorage::provider.
   *
   * @return name of cloud provider
   */
  virtual std::string name() const = 0;

  /**
   * @return host address to which cloud provider api's requests are called
   */
  virtual std::string endpoint() const = 0;

  /**
   * Returns the url to which user has to go in his web browser in order to give
   * consent to our library.
   *
   * @return authorization url
   */
  virtual std::string authorizeLibraryUrl() const = 0;

  /**
   * Returns IItem representing the root folder in cloud provider.
   *
   * @return root directory
   */
  virtual IItem::Pointer rootDirectory() const = 0;

  /**
   * Lists directory.
   *
   * @param directory directory to list
   * @return object representing the pending request
   */
  virtual ListDirectoryRequest::Pointer listDirectoryAsync(
      IItem::Pointer directory, IListDirectoryCallback::Pointer) = 0;

  /**
   * Tries to get the Item by its absolute path.
   *
   * @param absolute_path should start with /
   *
   * @param callback called when finished; if successful, it gets IItem
   * object as a parameter, otherwise it gets nullptr
   *
   * @return object representing the pending request
   */
  virtual GetItemRequest::Pointer getItemAsync(const std::string& absolute_path,
                                               GetItemCallback callback =
                                                   [](IItem::Pointer) {}) = 0;

  /**
   * Downloads the item, the file is provided by callback.
   *
   * @param item item to be downloaded
   * @return object representing the pending request
   */
  virtual DownloadFileRequest::Pointer downloadFileAsync(
      IItem::Pointer item, IDownloadFileCallback::Pointer) = 0;

  /**
   * Uploads the file provided by callback.
   *
   * @param parent parent of the uploaded file
   *
   * @param filename name at which the uploaded file will be saved in cloud
   * provider
   *
   * @return object representing the pending request
   */
  virtual UploadFileRequest::Pointer uploadFileAsync(
      IItem::Pointer parent, const std::string& filename,
      IUploadFileCallback::Pointer) = 0;

  /**
   * Retrieves IItem object from its id. That's the preferred way of updating
   * the IItem structure; IItem caches some data(e.g. thumbnail url or file url)
   * which may get invalidated over time, this function makes sure all IItem's
   * cached data is up to date.
   *
   * @param id
   *
   * @param callback called when finished; if successful, it gets
   * IItem object as a parameter, otherwise it gets nullptr
   *
   * @return object representing the pending request
   */
  virtual GetItemDataRequest::Pointer getItemDataAsync(
      const std::string& id,
      GetItemDataCallback callback = [](IItem::Pointer) {}) = 0;

  /**
   * Downloads thumbnail image, before calling the function, make sure provided
   * IItem is up to date.
   *
   * @param item
   * @return object representing the pending request
   */
  virtual DownloadFileRequest::Pointer getThumbnailAsync(
      IItem::Pointer item, IDownloadFileCallback::Pointer) = 0;

  /**
   * Deletes the item from cloud provider.
   *
   * @param item item to be deleted
   *
   * @param callback called when finished; if successful, it gets true,
   * otherwise false
   *
   * @return object representing the pending request
   */
  virtual DeleteItemRequest::Pointer deleteItemAsync(
      IItem::Pointer item, DeleteItemCallback callback = [](bool) {}) = 0;

  /**
   * Creates directory in cloud provider.
   *
   * @param parent parent directory
   *
   * @param name new directory's name
   *
   * @param callback called when finished; if successful, it gets IItem
   * object, otherwise nullptr
   *
   * @return object representing the pending request
   */
  virtual CreateDirectoryRequest::Pointer createDirectoryAsync(
      IItem::Pointer parent, const std::string& name,
      CreateDirectoryCallback callback = [](IItem::Pointer) {}) = 0;

  /**
   * Moves item.
   *
   * @param source file to be moved
   *
   * @param destination destination directory
   *
   * @param callback called when finished; if successful, it gets true,
   * otherwise false
   *
   * @return object representing the pending request
   */
  virtual MoveItemRequest::Pointer moveItemAsync(IItem::Pointer source,
                                                 IItem::Pointer destination,
                                                 MoveItemCallback callback =
                                                     [](bool) {}) = 0;

  /**
   * Renames item.
   *
   * @param item item to be renamed
   *
   * @param name new name
   *
   * @param callback called when finished; if successful, it gets true,
   * otherwise false
   *
   * @return object representing the pending request
   */
  virtual RenameItemRequest::Pointer renameItemAsync(
      IItem::Pointer item, const std::string& name,
      RenameItemCallback callback = [](bool) {}) = 0;

  /**
   * Simplified version of listDirectoryAsync.
   *
   * @param item directory to be listed
   * @param callback called when the request is finished
   * @return object representing the pending request
   */
  virtual ListDirectoryRequest::Pointer listDirectoryAsync(
      IItem::Pointer item,
      ListDirectoryCallback callback = [](const std::vector<IItem::Pointer>&) {
      }) = 0;

  /**
   * Simplified version of downloadFileAsync.
   *
   * @param item item to be downloaded
   *
   * @param filename name at which the downloaded file will be saved
   *
   * @param callback called when done; if successful, it gets true, otherwise
   * false
   *
   * @return object representing the pending request
   */
  virtual DownloadFileRequest::Pointer downloadFileAsync(
      IItem::Pointer item, const std::string& filename,
      DownloadFileCallback callback = [](bool) {}) = 0;

  /**
   * Simplified version of getThumbnailAsync.
   *
   * @param item
   *
   * @param filename name at which thumbnail will be saved
   *
   * @param callback called when done; if successful, it gets true, otherwise
   * false
   *
   * @return object representing pending request
   */
  virtual DownloadFileRequest::Pointer getThumbnailAsync(
      IItem::Pointer item, const std::string& filename,
      GetThumbnailCallback callback = [](bool) {}) = 0;

  /**
   * Simplified version of uploadFileAsync.
   *
   * @param parent parent of the uploaded file
   *
   * @param path path to the file which will be uploaded
   *
   * @param filename name at which the file will be saved in the cloud
   * provider
   *
   * @param callback called when done, if successful, it gets true, otherwise
   * false
   *
   * @return object representing the pending request
   */
  virtual UploadFileRequest::Pointer uploadFileAsync(
      IItem::Pointer parent, const std::string& path,
      const std::string& filename,
      UploadFileCallback callback = [](bool) {}) = 0;
};

}  // namespace cloudstorage

#endif  // ICLOUDPROVIDER_H
