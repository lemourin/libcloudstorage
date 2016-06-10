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

#include "Auth.h"
#include "ICloudProvider.h"

namespace cloudstorage {

class CloudProvider : public ICloudProvider {
 public:
  CloudProvider(IAuth::Pointer);

  bool initialize(ICallback::Pointer);
  Json::Value dump() const;

  std::string access_token() const;
  IAuth* auth() const;

  std::vector<IItem::Pointer> listDirectory(const IItem&) const final;
  void uploadFile(const std::string& filename, std::istream&) const final;
  void downloadFile(const IItem&, std::ostream&) const final;

  std::string authorizeLibraryUrl() const;
  IItem::Pointer rootDirectory() const;

  template <typename ReturnType, typename... FArgs, typename... Args>
  ReturnType execute(ReturnType (CloudProvider::*f)(FArgs...) const,
                     Args&&... args) const {
    try {
      if (!auth_->access_token()) auth_->authorize(auth_callback_.get());
      return (this->*f)(std::forward<Args>(args)...);
    } catch (const std::exception&) {
      if (!auth_->authorize(auth_callback_.get()))
        throw std::logic_error("Authorization failed.");
      return (this->*f)(std::forward<Args>(args)...);
    }
  }

 protected:
  virtual std::vector<IItem::Pointer> executeListDirectory(
      const IItem&) const = 0;
  virtual void executeUploadFile(const std::string& filename,
                                 std::istream&) const = 0;
  virtual void executeDownloadFile(const IItem&, std::ostream&) const = 0;

 private:
  IAuth::Pointer auth_;
  IAuth::ICallback::Pointer auth_callback_;
};

}  // namespace cloudstorage

#endif  //  CLOUDPROVIDER_H
