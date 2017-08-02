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

  using ExchangeCodeRequest = IRequest<EitherError<std::string>>;
  using ListDirectoryRequest =
      IRequest<EitherError<std::vector<IItem::Pointer>>>;
  using GetItemRequest = IRequest<EitherError<IItem>>;
  using DownloadFileRequest = IRequest<EitherError<void>>;
  using UploadFileRequest = IRequest<EitherError<void>>;
  using GetItemDataRequest = IRequest<EitherError<IItem>>;
  using DeleteItemRequest = IRequest<EitherError<void>>;
  using CreateDirectoryRequest = IRequest<EitherError<IItem>>;
  using MoveItemRequest = IRequest<EitherError<void>>;
  using RenameItemRequest = IRequest<EitherError<void>>;

  class IAuthCallback {
   public:
    using Pointer = std::shared_ptr<IAuthCallback>;

    enum class Status { WaitForAuthorizationCode, None };

    virtual ~IAuthCallback() = default;

    /**
     * Determines whether library should try to obtain authorization code or
     * not.
     *
     * @return Status::WaitForAuthorizationCode if the user wants to obtain the
     * authorization code, Status::None otherwise
     */
    virtual Status userConsentRequired(const ICloudProvider&) = 0;

    /**
     * Called when authorization is finished.
     */
    virtual void done(const ICloudProvider&, EitherError<void>) = 0;
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
    IAuthCallback::Pointer callback_;

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
    *  - redirect_uri_host
    *  - redirect_uri_port
    *  - state
    *  - access_token
    *  - daemon_port (used by mega.nz url provider)
    *  - file_url (used by mega.nz, url provider's base url)
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
   * Serializes token and hints in a json compact that are useful to be
   * restored in the following session. They might include refresh token, access
   * token or even custom provider configurations
   *
   * @param hints provider hints to be serialized
   * @param token token used to be serialized
   * @return serialized data with compact json
   */
  static std::string serializeSession(const std::string& token,
                                      const Hints& hints);

  /**
   * Deserializes token and hints from a json compact that were previously
   * serialized with serializeSession. They might include refresh token, access
   * token or even custom provider configurations
   *
   * @param serialized_data serialized session from previous session
   * @param hints pointer to the hints unserialized
   * @param token pointer to the token unserialized
   * @return returns false if fails to unserialized
   */
  static bool deserializeSession(const std::string& serialized_data,
                                 std::string& token, Hints& hints);

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
   * Exchanges authorization code which was sent to redirect_uri() by cloud
   * provider for a token (refresh_token).
   *
   * @return object representing the pending request
   */
  virtual ExchangeCodeRequest::Pointer exchangeCodeAsync(
      const std::string& code,
      ExchangeCodeCallback = [](EitherError<std::string>) {}) = 0;

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
  virtual GetItemRequest::Pointer getItemAsync(
      const std::string& absolute_path,
      GetItemCallback callback = [](EitherError<IItem>) {}) = 0;

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
      GetItemDataCallback callback = [](EitherError<IItem>) {}) = 0;

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
      IItem::Pointer item,
      DeleteItemCallback callback = [](EitherError<void>) {}) = 0;

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
      CreateDirectoryCallback callback = [](EitherError<IItem>) {}) = 0;

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
  virtual MoveItemRequest::Pointer moveItemAsync(
      IItem::Pointer source, IItem::Pointer destination,
      MoveItemCallback callback = [](EitherError<void>) {}) = 0;

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
      RenameItemCallback callback = [](EitherError<void>) {}) = 0;

  /**
   * Simplified version of listDirectoryAsync.
   *
   * @param item directory to be listed
   * @param callback called when the request is finished
   * @return object representing the pending request
   */
  virtual ListDirectoryRequest::Pointer listDirectoryAsync(
      IItem::Pointer item,
      ListDirectoryCallback callback =
          [](EitherError<std::vector<IItem::Pointer>>) {}) = 0;

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
      DownloadFileCallback callback = [](EitherError<void>) {}) = 0;

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
      GetThumbnailCallback callback = [](EitherError<void>) {}) = 0;

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
      UploadFileCallback callback = [](EitherError<void>) {}) = 0;
};

}  // namespace cloudstorage

#endif  // ICLOUDPROVIDER_H
