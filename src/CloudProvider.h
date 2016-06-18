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

#include <mutex>
#include "Auth.h"
#include "ICloudProvider.h"

namespace cloudstorage {

class CloudProvider : public ICloudProvider {
 public:
  CloudProvider(IAuth::Pointer);

  bool initialize(const std::string& token, ICallback::Pointer);

  std::string access_token() const;
  IAuth* auth() const;

  std::vector<IItem::Pointer> listDirectory(const IItem&) final;
  void uploadFile(const IItem& directory, const std::string& filename,
                  std::istream&) final;
  void downloadFile(const IItem&, std::ostream&) final;
  IItem::Pointer getItem(const std::string&) final;

  std::string authorizeLibraryUrl() const;
  std::string token() const;
  IItem::Pointer rootDirectory() const;

  template <typename ReturnType, typename... FArgs, typename... Args>
  ReturnType execute(ReturnType (CloudProvider::*f)(FArgs...) const,
                     Args&&... args) {
    try {
      {
        std::lock_guard<std::mutex> lock(auth_mutex_);
        if (!auth_->access_token()) auth_->authorize(auth_callback_.get());
      }
      return (this->*f)(std::forward<Args>(args)...);
    } catch (const std::exception&) {
      {
        std::lock_guard<std::mutex> lock(auth_mutex_);
        if (!auth_->authorize(auth_callback_.get()))
          throw std::logic_error("Authorization failed.");
      }
      return (this->*f)(std::forward<Args>(args)...);
    }
  }

 protected:
  virtual std::vector<IItem::Pointer> executeListDirectory(
      const IItem&) const = 0;
  virtual void executeUploadFile(const IItem& directory,
                                 const std::string& filename,
                                 std::istream&) const = 0;
  virtual void executeDownloadFile(const IItem&, std::ostream&) const = 0;

 private:
  IItem::Pointer getItem(std::vector<IItem::Pointer>&& items,
                         const std::string& filename) const;

  IAuth::Pointer auth_;
  IAuth::ICallback::Pointer auth_callback_;
  std::mutex auth_mutex_;
};

}  // namespace cloudstorage

#endif  //  CLOUDPROVIDER_H
