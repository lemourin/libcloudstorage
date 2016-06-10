/*****************************************************************************
 * Dropbox.h : prototypes for Dropbox
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

#ifndef DROPBOX_H
#define DROPBOX_H

#include "CloudProvider.h"

namespace cloudstorage {

class Dropbox : public CloudProvider {
 public:
  Dropbox();

  std::string name() const;
  std::string token() const;
  IItem::Pointer rootDirectory() const;
  std::vector<IItem::Pointer> executeListDirectory(const IItem&) const;
  void executeUploadFile(const std::string& filename, std::istream&) const;
  void executeDownloadFile(const IItem&, std::ostream&) const;

 private:
  class Auth : public cloudstorage::Auth {
   public:
    Auth();

    std::string authorizeLibraryUrl() const;
    std::string requestAuthorizationCode() const;
    Token::Pointer requestAccessToken() const;
    Token::Pointer refreshToken() const;
    bool validateToken(Token&) const;
    Token::Pointer fromTokenString(const std::string&) const;
  };
};

}  // namespace cloudstorage

#endif  // DROPBOX_H
