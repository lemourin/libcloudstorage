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

  void initialize(const std::string& token, ICallback::Pointer,
                  const Hints& hints);
  Hints hints() const;
  std::string name() const;

 private:
  HttpRequest::Pointer getItemDataRequest(const std::string&,
                                          std::ostream&) const;
  HttpRequest::Pointer listDirectoryRequest(const IItem&,
                                            const std::string& page_token,
                                            std::ostream& input_stream) const;
  HttpRequest::Pointer uploadFileRequest(const IItem& directory,
                                         const std::string& filename,
                                         std::istream& stream,
                                         std::ostream& input_stream) const;
  HttpRequest::Pointer downloadFileRequest(const IItem&,
                                           std::ostream& input_stream) const;

  IItem::Pointer getItemDataResponse(std::istream& response) const;
  std::vector<IItem::Pointer> listDirectoryResponse(
      std::istream&, std::string& next_page_token) const;

  IItem::Pointer toItem(const Json::Value&, std::string kind) const;

  class Auth : public cloudstorage::GoogleDrive::Auth {
   public:
    std::string authorizeLibraryUrl() const;
  };

  std::string youtube_dl_url_;
};

}  // namespace cloudstorage

#endif  // YOUTUBE_H
