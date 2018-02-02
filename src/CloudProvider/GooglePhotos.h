/*****************************************************************************
 * GoogleDrive.h
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
#ifndef GOOGLEPHOTOS_H
#define GOOGLEPHOTOS_H

#include <tinyxml2.h>
#include "GoogleDrive.h"

namespace cloudstorage {

class GooglePhotos : public CloudProvider {
 public:
  GooglePhotos();
  std::string name() const override;
  std::string endpoint() const override;
  IItem::Pointer rootDirectory() const override;
  bool reauthorize(int code,
                   const IHttpRequest::HeaderParameters&) const override;
  OperationSet supportedOperations() const override;

  IHttpRequest::Pointer getItemDataRequest(
      const std::string&, std::ostream& input_stream) const override;
  IItem::Pointer getItemDataResponse(std::istream& response) const override;
  IHttpRequest::Pointer listDirectoryRequest(
      const IItem&, const std::string& page_token,
      std::ostream& input_stream) const override;
  IHttpRequest::Pointer uploadFileRequest(
      const IItem& directory, const std::string& filename,
      std::ostream& prefix_stream, std::ostream& suffix_stream) const override;
  IHttpRequest::Pointer getGeneralDataRequest(std::ostream&) const override;
  std::vector<IItem::Pointer> listDirectoryResponse(
      const IItem&, std::istream&, std::string& next_page_token) const override;
  GeneralData getGeneralDataResponse(std::istream& response) const override;
  DownloadFileRequest::Pointer downloadFileAsync(IItem::Pointer,
                                                 IDownloadFileCallback::Pointer,
                                                 Range) override;

  IItem::Pointer toItem(const tinyxml2::XMLElement*) const;

  class Auth : public cloudstorage::GoogleDrive::Auth {
   public:
    std::string authorizeLibraryUrl() const override;
  };
};

}  // namespace cloudstorage
#endif  // GOOGLEPHOTOS_H
