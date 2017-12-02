/*****************************************************************************
 * YouTube.h : YouTube headers
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

#ifndef YOUTUBE_H
#define YOUTUBE_H

#include "CloudProvider/CloudProvider.h"
#include "GoogleDrive.h"
#include "Utility/Item.h"

namespace cloudstorage {

class YouTube : public CloudProvider {
 public:
  YouTube();

  IItem::Pointer rootDirectory() const override;
  std::string name() const override;
  std::string endpoint() const override;

  GetItemDataRequest::Pointer getItemDataAsync(const std::string& id,
                                               GetItemDataCallback f) override;
  ListDirectoryPageRequest::Pointer listDirectoryPageAsync(
      IItem::Pointer, const std::string&, ListDirectoryPageCallback) override;
  ListDirectoryRequest::Pointer listDirectoryAsync(
      IItem::Pointer item, IListDirectoryCallback::Pointer callback) override;
  DownloadFileRequest::Pointer downloadFileAsync(IItem::Pointer,
                                                 IDownloadFileCallback::Pointer,
                                                 Range) override;

 private:
  GetItemUrlRequest::Pointer getItemUrlAsync(IItem::Pointer,
                                             GetItemUrlCallback) override;
  IHttpRequest::Pointer listDirectoryRequest(
      const IItem&, const std::string& page_token,
      std::ostream& input_stream) const override;

  IHttpRequest::Pointer getItemDataRequest(const std::string& full_id,
                                           std::ostream&) const override;
  IItem::Pointer getItemDataResponse(std::istream& response, bool audio,
                                     bool high_quality) const;
  std::vector<IItem::Pointer> listDirectoryResponse(
      const IItem& directory, std::istream&,
      std::string& next_page_token) const override;

  IItem::Pointer toItem(const Json::Value&, std::string kind, bool audio,
                        bool high_quality) const;

  class Auth : public cloudstorage::GoogleDrive::Auth {
   public:
    std::string authorizeLibraryUrl() const override;
  };
};

}  // namespace cloudstorage

#endif  // YOUTUBE_H
