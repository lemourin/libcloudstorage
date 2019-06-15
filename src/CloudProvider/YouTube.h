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

struct VideoInfo {
  std::string scrambled_signature;
  std::string signature;
  std::string type;
  std::string url;
  int bitrate;
  std::string codec;
  std::string quality_label;
  std::string init_range;
  std::string index_range;
  uint64_t size = 0;
  uint32_t width;
  uint32_t height;
  uint32_t framerate;
  uint32_t samplerate;
  bool adaptive;
  std::string signature_keyname = "signature";
};

class YouTube : public CloudProvider {
 public:
  YouTube();

  IItem::Pointer rootDirectory() const override;
  std::string name() const override;
  std::string endpoint() const override;
  OperationSet supportedOperations() const override;
  bool isSuccess(int, const IHttpRequest::HeaderParameters&) const override;

  GetItemDataRequest::Pointer getItemDataAsync(const std::string& id,
                                               GetItemDataCallback f) override;
  DownloadFileRequest::Pointer downloadFileAsync(IItem::Pointer,
                                                 IDownloadFileCallback::Pointer,
                                                 Range) override;

 private:
  GeneralDataRequest::Pointer getGeneralDataAsync(GeneralDataCallback) override;
  GetItemUrlRequest::Pointer getItemUrlAsync(IItem::Pointer,
                                             GetItemUrlCallback) override;
  IHttpRequest::Pointer listDirectoryRequest(
      const IItem&, const std::string& page_token,
      std::ostream& input_stream) const override;

  IHttpRequest::Pointer deleteItemRequest(
      const IItem&, std::ostream& input_stream) const override;
  IHttpRequest::Pointer renameItemRequest(const IItem& item,
                                          const std::string& name,
                                          std::ostream&) const override;
  IHttpRequest::Pointer createDirectoryRequest(const IItem& item,
                                               const std::string& name,
                                               std::ostream&) const override;
  IItem::List listDirectoryResponse(
      const IItem& directory, std::istream&,
      std::string& next_page_token) const override;
  IItem::Pointer renameItemResponse(const IItem& old_item,
                                    const std::string& name,
                                    std::istream& response) const override;
  IItem::Pointer createDirectoryResponse(const IItem& parent,
                                         const std::string& name,
                                         std::istream& response) const override;
  std::string generateDashManifest(
      const IItem& i, const std::vector<VideoInfo>& video_info) const;

  Item::Pointer toItem(const Json::Value&, std::string kind, int type) const;

  class Auth : public cloudstorage::GoogleDrive::Auth {
   public:
    std::string authorizeLibraryUrl() const override;
  };

  mutable util::LRUCache<std::string, std::vector<VideoInfo>> manifest_data_;
};

}  // namespace cloudstorage

#endif  // YOUTUBE_H
