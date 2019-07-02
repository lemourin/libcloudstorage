/*****************************************************************************
 * CloudProvider.cpp
 *
 *****************************************************************************
 * Copyright (C) 2016-2018 VideoLAN
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
#include "CloudStorage.h"

#include <cstring>

#include "ICloudProvider.h"
#include "Utility/Utility.h"

using namespace cloudstorage;

uint32_t cloud_provider_supported_operations(cloud_provider *d) {
  return reinterpret_cast<ICloudProvider *>(d)->supportedOperations();
}

void cloud_provider_release(cloud_provider *d) {
  delete reinterpret_cast<ICloudProvider *>(d);
}

cloud_string *cloud_provider_token(cloud_provider *d) {
  return cloud_string_create(
      reinterpret_cast<ICloudProvider *>(d)->token().c_str());
}

cloud_hints *cloud_provider_hints(cloud_provider *d) {
  return reinterpret_cast<cloud_hints *>(new ICloudProvider::Hints(
      reinterpret_cast<ICloudProvider *>(d)->hints()));
}

void cloud_hints_release(cloud_hints *d) {
  delete reinterpret_cast<ICloudProvider::Hints *>(d);
}

cloud_string *cloud_provider_name(cloud_provider *d) {
  return cloud_string_create(
      reinterpret_cast<ICloudProvider *>(d)->name().c_str());
}

cloud_string *cloud_provider_endpoint(cloud_provider *d) {
  return cloud_string_create(
      reinterpret_cast<ICloudProvider *>(d)->endpoint().c_str());
}

cloud_string *cloud_provider_authorize_library_url(const cloud_provider *d) {
  return cloud_string_create(reinterpret_cast<const ICloudProvider *>(d)
                                 ->authorizeLibraryUrl()
                                 .c_str());
}

cloud_item *cloud_provider_root_directory(const cloud_provider *d) {
  return reinterpret_cast<cloud_item *>(new IItem::Pointer(
      reinterpret_cast<const ICloudProvider *>(d)->rootDirectory()));
}

cloud_request_exchange_code *cloud_provider_exchange_code(
    cloud_provider *p, const char *code,
    cloud_request_exchange_code_callback callback, void *data) {
  return reinterpret_cast<cloud_request_exchange_code *>(
      reinterpret_cast<ICloudProvider *>(p)
          ->exchangeCodeAsync(
              code,
              [=](EitherError<Token> e) {
                if (callback)
                  callback(reinterpret_cast<cloud_either_token *>(&e), data);
              })
          .release());
}

cloud_request_get_item_url *cloud_provider_get_item_url(
    cloud_provider *p, cloud_item *item,
    cloud_request_get_item_url_callback callback, void *data) {
  return reinterpret_cast<cloud_request_get_item_url *>(
      reinterpret_cast<ICloudProvider *>(p)
          ->getItemUrlAsync(
              *reinterpret_cast<IItem::Pointer *>(item),
              [=](EitherError<std::string> e) {
                if (callback)
                  callback(reinterpret_cast<cloud_either_string *>(&e), data);
              })
          .release());
}

cloud_request_list_directory *cloud_provider_list_directory(
    cloud_provider *p, cloud_item *item,
    cloud_request_list_directory_callback *callback, void *data) {
  class ListDirectoryCallback : public IListDirectoryCallback {
   public:
    ListDirectoryCallback(cloud_request_list_directory_callback callback,
                          void *data)
        : callback_(callback), data_(data) {}

    void receivedItem(IItem::Pointer item) override {
      if (callback_.received_item)
        callback_.received_item(reinterpret_cast<cloud_item *>(&item), data_);
    }

    void done(EitherError<std::vector<IItem::Pointer>> e) override {
      if (callback_.done)
        callback_.done(reinterpret_cast<cloud_either_item_list *>(&e), data_);
    }

   private:
    cloud_request_list_directory_callback callback_;
    void *data_;
  };
  return reinterpret_cast<cloud_request_list_directory *>(
      reinterpret_cast<ICloudProvider *>(p)
          ->listDirectoryAsync(
              *reinterpret_cast<IItem::Pointer *>(item),
              util::make_unique<ListDirectoryCallback>(
                  callback ? *callback
                           : cloud_request_list_directory_callback{},
                  data))
          .release());
}

cloud_request_get_item *cloud_provider_get_item(
    cloud_provider *p, cloud_string *path,
    cloud_request_get_item_callback callback, void *data) {
  return reinterpret_cast<cloud_request_get_item *>(
      reinterpret_cast<ICloudProvider *>(p)
          ->getItemAsync(path,
                         [=](EitherError<IItem> e) {
                           if (callback)
                             callback(reinterpret_cast<cloud_either_item *>(&e),
                                      data);
                         })
          .release());
}

void cloud_provider_init_data_release(cloud_provider_init_data *d) {
  delete reinterpret_cast<ICloudProvider::InitData *>(d);
}

cloud_provider_init_data *cloud_provider_init_data_create(
    const struct cloud_provider_init_data_args *d) {
  class AuthCallback : public ICloudProvider::IAuthCallback {
   public:
    AuthCallback(cloud_provider_auth_callback callback, void *data)
        : callback_(callback), data_(data) {}

    Status userConsentRequired(const ICloudProvider &p) override {
      return static_cast<Status>(callback_.user_consent_required(
          reinterpret_cast<const cloud_provider *>(&p), data_));
    }

    void done(const ICloudProvider &, EitherError<void> e) override {
      callback_.done(reinterpret_cast<cloud_either_void *>(&e), data_);
    }

   private:
    cloud_provider_auth_callback callback_;
    void *data_;
  };
  auto init_data = new ICloudProvider::InitData;
  if (d->token) init_data->token_ = d->token;
  if (d->auth_callback.user_consent_required && d->auth_callback.done)
    init_data->callback_ =
        util::make_unique<AuthCallback>(d->auth_callback, d->auth_data);
  init_data->permission_ =
      static_cast<ICloudProvider::Permission>(d->permission);
  if (d->crypto)
    init_data->crypto_engine_ =
        std::unique_ptr<ICrypto>(reinterpret_cast<ICrypto *>(d->crypto));
  if (d->http)
    init_data->http_engine_ =
        std::unique_ptr<IHttp>(reinterpret_cast<IHttp *>(d->http));
  if (d->http_server)
    init_data->http_server_ = std::unique_ptr<IHttpServerFactory>(
        reinterpret_cast<IHttpServerFactory *>(d->http_server));
  if (d->thread_pool)
    init_data->thread_pool_ = std::unique_ptr<IThreadPool>(
        reinterpret_cast<IThreadPool *>(d->thread_pool));
  if (d->hints)
    init_data->hints_ = *reinterpret_cast<ICloudProvider::Hints *>(d->hints);
  return reinterpret_cast<cloud_provider_init_data *>(init_data);
}

cloud_hints *cloud_hints_create() {
  return reinterpret_cast<cloud_hints *>(new ICloudProvider::Hints());
}

void cloud_hints_add(cloud_hints *hints, cloud_string *key,
                     cloud_string *value) {
  (*reinterpret_cast<ICloudProvider::Hints *>(hints))[key] = value;
}

cloud_string *cloud_provider_token(const cloud_provider *p) {
  const auto provider = reinterpret_cast<const ICloudProvider *>(p);
  return cloud_string_create(provider->token().c_str());
}

cloud_hints *cloud_provider_hints(const cloud_provider *p) {
  const auto provider = reinterpret_cast<const ICloudProvider *>(p);
  return reinterpret_cast<cloud_hints *>(
      new ICloudProvider::Hints(provider->hints()));
}

cloud_string *cloud_provider_serialize_session(cloud_string *token,
                                               const cloud_hints *hints) {
  return cloud_string_create(
      ICloudProvider::serializeSession(
          token, *reinterpret_cast<const ICloudProvider::Hints *>(hints))
          .c_str());
}

int cloud_provider_deserialize_session(cloud_string *data, cloud_string **token,
                                       cloud_hints **hints) {
  std::string token_cpp;
  std::unique_ptr<ICloudProvider::Hints> hints_cpp =
      util::make_unique<ICloudProvider::Hints>();
  if (!ICloudProvider::deserializeSession(data, token_cpp, *hints_cpp)) {
    return -1;
  }
  *token = cloud_string_create(token_cpp.c_str());
  *hints = reinterpret_cast<cloud_hints *>(hints_cpp.release());
  return 0;
}

cloud_request_get_item_data *cloud_provider_get_item_data(
    cloud_provider *provider, cloud_string *id,
    cloud_request_get_item_data_callback callback, void *data) {
  return reinterpret_cast<cloud_request_get_item_data *>(
      reinterpret_cast<ICloudProvider *>(provider)
          ->getItemDataAsync(
              id,
              [=](EitherError<IItem> e) {
                if (callback)
                  callback(reinterpret_cast<cloud_either_item *>(&e), data);
              })
          .release());
}

cloud_request_create_directory *cloud_provider_create_directory(
    cloud_provider *provider, cloud_item *parent, cloud_string *name,
    cloud_request_create_directory_callback callback, void *data) {
  return reinterpret_cast<cloud_request_create_directory *>(
      reinterpret_cast<ICloudProvider *>(provider)
          ->createDirectoryAsync(
              *reinterpret_cast<IItem::Pointer *>(parent), name,
              [=](EitherError<IItem> e) {
                if (callback)
                  callback(reinterpret_cast<cloud_either_item *>(&e), data);
              })
          .release());
}

cloud_request_delete_item *cloud_provider_delete_item(
    cloud_provider *provider, cloud_item *item,
    cloud_request_delete_item_callback callback, void *data) {
  return reinterpret_cast<cloud_request_delete_item *>(
      reinterpret_cast<ICloudProvider *>(provider)
          ->deleteItemAsync(
              *reinterpret_cast<IItem::Pointer *>(item),
              [=](EitherError<void> e) {
                if (callback)
                  callback(reinterpret_cast<cloud_either_void *>(&e), data);
              })
          .release());
}

cloud_request_rename_item *cloud_provider_rename_item(
    cloud_provider *provider, cloud_item *item, cloud_string *new_name,
    cloud_request_rename_item_callback callback, void *data) {
  return reinterpret_cast<cloud_request_rename_item *>(
      reinterpret_cast<ICloudProvider *>(provider)
          ->renameItemAsync(
              *reinterpret_cast<IItem::Pointer *>(item), new_name,
              [=](EitherError<IItem> e) {
                if (callback)
                  callback(reinterpret_cast<cloud_either_item *>(&e), data);
              })
          .release());
}

cloud_request_move_item *cloud_provider_move_item(
    cloud_provider *provider, cloud_item *item, cloud_item *new_parent,
    cloud_request_move_item_callback callback, void *data) {
  return reinterpret_cast<cloud_request_move_item *>(
      reinterpret_cast<ICloudProvider *>(provider)
          ->moveItemAsync(
              *reinterpret_cast<IItem::Pointer *>(item),
              *reinterpret_cast<IItem::Pointer *>(new_parent),
              [=](EitherError<IItem> e) {
                if (callback)
                  callback(reinterpret_cast<cloud_either_item *>(&e), data);
              })
          .release());
}

cloud_request_list_directory *cloud_provider_list_directory_simple(
    cloud_provider *provider, cloud_item *item,
    cloud_request_list_directory_simple_callback callback, void *data) {
  return reinterpret_cast<cloud_request_list_directory *>(
      reinterpret_cast<ICloudProvider *>(provider)
          ->listDirectorySimpleAsync(
              *reinterpret_cast<IItem::Pointer *>(item),
              [=](EitherError<IItem::List> e) {
                if (callback)
                  callback(reinterpret_cast<cloud_either_item_list *>(&e),
                           data);
              })
          .release());
}

cloud_request_list_directory_page *cloud_provider_list_directory_page(
    cloud_provider *provider, cloud_item *item, cloud_string *token,
    cloud_request_list_directory_page_callback callback, void *data) {
  return reinterpret_cast<cloud_request_list_directory_page *>(
      reinterpret_cast<ICloudProvider *>(provider)
          ->listDirectoryPageAsync(
              *reinterpret_cast<IItem::Pointer *>(item), token,
              [=](EitherError<PageData> e) {
                if (callback)
                  callback(reinterpret_cast<cloud_either_page_data *>(&e),
                           data);
              })
          .release());
}

cloud_request_get_general_data *cloud_provider_get_general_data(
    cloud_provider *provider, cloud_request_get_general_data_callback callback,
    void *data) {
  return reinterpret_cast<cloud_request_get_general_data *>(
      reinterpret_cast<ICloudProvider *>(provider)
          ->getGeneralDataAsync([=](EitherError<GeneralData> e) {
            if (callback)
              callback(reinterpret_cast<cloud_either_general_data *>(&e), data);
          })
          .release());
}

cloud_request_get_item_url *cloud_provider_get_file_daemon_url(
    cloud_provider *provider, cloud_item *item,
    cloud_request_get_item_url_callback callback, void *data) {
  return reinterpret_cast<cloud_request_get_item_url *>(
      reinterpret_cast<ICloudProvider *>(provider)
          ->getItemUrlAsync(
              *reinterpret_cast<IItem::Pointer *>(item),
              [=](EitherError<std::string> e) {
                if (callback)
                  callback(reinterpret_cast<cloud_either_string *>(&e), data);
              })
          .release());
}

cloud_request_upload_file *cloud_provider_upload_file(
    cloud_provider *provider, cloud_item *parent, cloud_string *filename,
    cloud_request_upload_file_callback *cb, void *data) {
  struct UploadCallback : public IUploadFileCallback {
    UploadCallback(cloud_request_upload_file_callback callback, void *data)
        : callback_(callback), data_(data) {}

    uint32_t putData(char *data, uint32_t maxlength, uint64_t offset) override {
      return callback_.put_data(data, maxlength, offset, data_);
    }

    uint64_t size() override { return callback_.size(data_); }

    void progress(uint64_t total, uint64_t now) override {
      if (callback_.progress) callback_.progress(total, now, data_);
    }

    void done(EitherError<IItem> e) override {
      if (callback_.done)
        callback_.done(reinterpret_cast<cloud_either_item *>(&e), data_);
    }

    cloud_request_upload_file_callback callback_;
    void *data_;
  };

  return reinterpret_cast<cloud_request_upload_file *>(
      reinterpret_cast<ICloudProvider *>(provider)
          ->uploadFileAsync(
              *reinterpret_cast<IItem::Pointer *>(parent), filename,
              std::make_shared<UploadCallback>(
                  cb ? *cb : cloud_request_upload_file_callback{}, data))
          .release());
}

namespace {
struct DownloadCallback : public IDownloadFileCallback {
  DownloadCallback(cloud_request_download_file_callback callback, void *data)
      : callback_(callback), data_(data) {}

  void receivedData(const char *data, uint32_t length) override {
    if (callback_.received_data) callback_.received_data(data, length, data_);
  }

  void progress(uint64_t total, uint64_t now) override {
    if (callback_.progress) callback_.progress(total, now, data_);
  }

  void done(EitherError<void> e) override {
    if (callback_.done)
      callback_.done(reinterpret_cast<cloud_either_void *>(&e), data_);
  }

  cloud_request_download_file_callback callback_;
  void *data_;
};
}  // namespace

cloud_request_download_file *cloud_provider_download_file(
    cloud_provider *provider, cloud_item *item, cloud_range range,
    cloud_request_download_file_callback *callback, void *data) {
  return reinterpret_cast<cloud_request_download_file *>(
      reinterpret_cast<ICloudProvider *>(provider)
          ->downloadFileAsync(
              *reinterpret_cast<IItem::Pointer *>(item),
              std::make_shared<DownloadCallback>(
                  callback ? *callback : cloud_request_download_file_callback{},
                  data),
              Range{range.start, range.size})
          .release());
}

cloud_request_download_file *cloud_provider_get_thumbnail(
    cloud_provider *provider, cloud_item *item,
    cloud_request_download_file_callback *callback, void *data) {
  return reinterpret_cast<cloud_request_download_file *>(
      reinterpret_cast<ICloudProvider *>(provider)
          ->getThumbnailAsync(
              *reinterpret_cast<IItem::Pointer *>(item),
              std::make_shared<DownloadCallback>(
                  callback ? *callback : cloud_request_download_file_callback{},
                  data))
          .release());
}
