/*****************************************************************************
 * Request.cpp
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
#include "Request.h"
#include "ICloudProvider.h"
#include "IRequest.h"

#include <string.h>

using namespace cloudstorage;

void cloud_request_release(cloud_request *r) {
  delete reinterpret_cast<IGenericRequest *>(r);
}

void cloud_request_finish(cloud_request *r) {
  reinterpret_cast<IGenericRequest *>(r)->finish();
}

cloud_string *cloud_error_description(cloud_error *d) {
  return strdup(
      (*reinterpret_cast<std::shared_ptr<Error> *>(d))->description_.c_str());
}

int cloud_error_code(cloud_error *d) {
  return (*reinterpret_cast<std::shared_ptr<Error> *>(d))->code_;
}

void cloud_error_release(cloud_error *d) {
  delete reinterpret_cast<std::shared_ptr<Error> *>(d);
}

cloud_string *cloud_token_token(cloud_token *d) {
  return strdup(
      (*reinterpret_cast<std::shared_ptr<Token> *>(d))->token_.c_str());
}

cloud_string *cloud_token_access_token(struct cloud_token *d) {
  return strdup(
      (*reinterpret_cast<std::shared_ptr<Token> *>(d))->access_token_.c_str());
}

void cloud_token_release(cloud_token *d) {
  delete reinterpret_cast<std::shared_ptr<Token> *>(d);
}

int cloud_item_list_length(const cloud_item_list *d) {
  auto r = reinterpret_cast<const std::shared_ptr<IItem::List> *>(d);
  return (*r)->size();
}

cloud_item *cloud_item_list_get(const cloud_item_list *d, int index) {
  auto r = reinterpret_cast<const std::shared_ptr<IItem::List> *>(d);
  return reinterpret_cast<cloud_item *>(new IItem::Pointer((*r)->at(index)));
}

void cloud_item_list_release(cloud_item_list *d) {
  delete reinterpret_cast<std::shared_ptr<IItem::List> *>(d);
}

cloud_string *cloud_page_next_token(cloud_page_data *d) {
  return strdup(
      (*reinterpret_cast<std::shared_ptr<PageData> *>(d))->next_token_.c_str());
}

cloud_item_list *cloud_page_item_list(cloud_page_data *d) {
  return reinterpret_cast<cloud_item_list *>(
      new std::shared_ptr<IItem::List>(std::make_shared<IItem::List>(
          (*reinterpret_cast<std::shared_ptr<PageData> *>(d))->items_)));
}

void cloud_page_data_release(cloud_page_data *d) {
  delete reinterpret_cast<std::shared_ptr<PageData> *>(d);
}

cloud_string *cloud_general_data_username(struct cloud_general_data *d) {
  return strdup((*reinterpret_cast<std::shared_ptr<GeneralData> *>(d))
                    ->username_.c_str());
}

uint64_t cloud_general_data_space_total(struct cloud_general_data *d) {
  return (*reinterpret_cast<std::shared_ptr<GeneralData> *>(d))->space_total_;
}

uint64_t cloud_general_data_space_used(struct cloud_general_data *d) {
  return (*reinterpret_cast<std::shared_ptr<GeneralData> *>(d))->space_used_;
}

void cloud_general_data_release(cloud_general_data *d) {
  delete reinterpret_cast<std::shared_ptr<GeneralData> *>(d);
}

cloud_either_item_list *cloud_request_list_directory_result(
    cloud_request_list_directory *d) {
  auto r = reinterpret_cast<ICloudProvider::ListDirectoryRequest *>(d);
  return reinterpret_cast<cloud_either_item_list *>(
      new EitherError<IItem::List>(r->result()));
}

cloud_either_token *cloud_request_exchange_code_result(
    cloud_request_exchange_code *d) {
  auto r = reinterpret_cast<ICloudProvider::ExchangeCodeRequest *>(d);
  return reinterpret_cast<cloud_either_token *>(
      new EitherError<Token>(r->result()));
}

cloud_either_string *cloud_request_get_item_url_result(
    cloud_request_get_item_url *d) {
  auto r = reinterpret_cast<ICloudProvider::GetItemUrlRequest *>(d);
  return reinterpret_cast<cloud_either_string *>(
      new EitherError<std::string>(r->result()));
}

cloud_either_item *cloud_request_get_item_result(cloud_request_get_item *d) {
  auto r = reinterpret_cast<ICloudProvider::GetItemRequest *>(d);
  return reinterpret_cast<cloud_either_item *>(
      new EitherError<IItem>(r->result()));
}

cloud_either_item *cloud_request_get_item_data_result(
    cloud_request_get_item_data *d) {
  auto r = reinterpret_cast<ICloudProvider::GetItemDataRequest *>(d);
  return reinterpret_cast<cloud_either_item *>(
      new EitherError<IItem>(r->result()));
}

cloud_either_item *cloud_request_create_directory_result(
    cloud_request_create_directory *d) {
  auto r = reinterpret_cast<ICloudProvider::CreateDirectoryRequest *>(d);
  return reinterpret_cast<cloud_either_item *>(
      new EitherError<IItem>(r->result()));
}

cloud_either_void *cloud_request_delete_item_result(
    cloud_request_delete_item *d) {
  auto r = reinterpret_cast<ICloudProvider::DeleteItemRequest *>(d);
  return reinterpret_cast<cloud_either_void *>(
      new EitherError<void>(r->result()));
}

cloud_either_item *cloud_request_rename_item_result(
    cloud_request_rename_item *d) {
  auto r = reinterpret_cast<ICloudProvider::RenameItemRequest *>(d);
  return reinterpret_cast<cloud_either_item *>(
      new EitherError<IItem>(r->result()));
}

cloud_either_item *cloud_request_move_item_result(cloud_request_move_item *d) {
  auto r = reinterpret_cast<ICloudProvider::MoveItemRequest *>(d);
  return reinterpret_cast<cloud_either_item *>(
      new EitherError<IItem>(r->result()));
}

cloud_either_page_data *cloud_request_list_directory_page_result(
    cloud_request_list_directory_page *d) {
  auto r = reinterpret_cast<ICloudProvider::ListDirectoryPageRequest *>(d);
  return reinterpret_cast<cloud_either_page_data *>(
      new EitherError<PageData>(r->result()));
}

cloud_either_general_data *cloud_request_get_general_data_result(
    cloud_request_get_general_data *d) {
  auto r = reinterpret_cast<ICloudProvider::GeneralDataRequest *>(d);
  return reinterpret_cast<cloud_either_general_data *>(
      new EitherError<GeneralData>(r->result()));
}

cloud_either_item *cloud_request_upload_file_result(
    cloud_request_upload_file *d) {
  auto r = reinterpret_cast<ICloudProvider::UploadFileRequest *>(d);
  return reinterpret_cast<cloud_either_item *>(
      new EitherError<IItem>(r->result()));
}

cloud_either_void *cloud_request_download_file_result(
    cloud_request_download_file *d) {
  auto r = reinterpret_cast<ICloudProvider::DownloadFileRequest *>(d);
  return reinterpret_cast<cloud_either_void *>(
      new EitherError<void>(r->result()));
}

cloud_error *cloud_either_token_error(cloud_either_token *d) {
  auto r = reinterpret_cast<EitherError<Token> *>(d);
  if (!r->left()) return nullptr;
  return reinterpret_cast<cloud_error *>(new std::shared_ptr<Error>(r->left()));
}

cloud_token *cloud_either_token_result(cloud_either_token *d) {
  auto r = reinterpret_cast<EitherError<Token> *>(d);
  if (!r->right()) return nullptr;
  return reinterpret_cast<cloud_token *>(
      new std::shared_ptr<Token>(r->right()));
}

void cloud_either_token_release(cloud_either_token *d) {
  delete reinterpret_cast<EitherError<Token> *>(d);
}

cloud_error *cloud_either_item_list_error(cloud_either_item_list *d) {
  auto r = reinterpret_cast<EitherError<IItem::List> *>(d);
  if (!r->left()) return nullptr;
  return reinterpret_cast<cloud_error *>(new std::shared_ptr<Error>(r->left()));
}

cloud_item_list *cloud_either_item_list_result(cloud_either_item_list *d) {
  auto r = reinterpret_cast<EitherError<IItem::List> *>(d);
  if (!r->right()) return nullptr;
  return reinterpret_cast<cloud_item_list *>(
      new std::shared_ptr<IItem::List>(r->right()));
}

void cloud_either_item_list_release(cloud_either_item_list *d) {
  delete reinterpret_cast<EitherError<IItem::List> *>(d);
}

cloud_error *cloud_either_void_error(cloud_either_void *d) {
  auto r = reinterpret_cast<EitherError<void> *>(d);
  if (!r->left()) return nullptr;
  return reinterpret_cast<cloud_error *>(new std::shared_ptr<Error>(r->left()));
}

void cloud_either_void_release(cloud_either_void *d) {
  delete reinterpret_cast<EitherError<void> *>(d);
}

cloud_error *cloud_either_string_error(cloud_either_string *d) {
  auto r = reinterpret_cast<EitherError<std::string> *>(d);
  if (!r->left()) return nullptr;
  return reinterpret_cast<cloud_error *>(new std::shared_ptr<Error>(r->left()));
}

cloud_string *cloud_either_string_result(cloud_either_string *d) {
  auto r = reinterpret_cast<EitherError<std::string> *>(d);
  if (!r->right()) return nullptr;
  return reinterpret_cast<cloud_string *>(strdup(r->right()->c_str()));
}

void cloud_either_string_release(cloud_either_string *d) {
  delete reinterpret_cast<EitherError<std::string> *>(d);
}

cloud_error *cloud_either_item_error(cloud_either_item *d) {
  auto r = reinterpret_cast<EitherError<IItem> *>(d);
  if (!r->left()) return nullptr;
  return reinterpret_cast<cloud_error *>(new std::shared_ptr<Error>(r->left()));
}

cloud_item *cloud_either_item_result(cloud_either_item *d) {
  auto r = reinterpret_cast<EitherError<IItem> *>(d);
  if (!r->right()) return nullptr;
  return reinterpret_cast<cloud_item *>(new IItem::Pointer(r->right()));
}

void cloud_either_item_release(cloud_either_item *d) {
  delete reinterpret_cast<EitherError<IItem> *>(d);
}

cloud_error *cloud_either_page_data_error(cloud_either_page_data *d) {
  auto r = reinterpret_cast<EitherError<PageData> *>(d);
  if (!r->left()) return nullptr;
  return reinterpret_cast<cloud_error *>(new std::shared_ptr<Error>(r->left()));
}

cloud_page_data *cloud_either_page_data_result(cloud_either_page_data *d) {
  auto r = reinterpret_cast<EitherError<PageData> *>(d);
  if (!r->right()) return nullptr;
  return reinterpret_cast<cloud_page_data *>(
      new std::shared_ptr<PageData>(r->right()));
}

void cloud_either_page_data_release(cloud_either_page_data *d) {
  delete reinterpret_cast<EitherError<PageData> *>(d);
}

cloud_error *cloud_either_general_data_error(cloud_either_general_data *d) {
  auto r = reinterpret_cast<EitherError<GeneralData> *>(d);
  if (!r->left()) return nullptr;
  return reinterpret_cast<cloud_error *>(new std::shared_ptr<Error>(r->left()));
}

cloud_general_data *cloud_either_general_data_result(
    cloud_either_general_data *d) {
  auto r = reinterpret_cast<EitherError<GeneralData> *>(d);
  if (!r->right()) return nullptr;
  return reinterpret_cast<cloud_general_data *>(
      new std::shared_ptr<GeneralData>(r->right()));
}

void cloud_either_general_data_release(cloud_either_general_data *d) {
  delete reinterpret_cast<EitherError<GeneralData> *>(d);
}
