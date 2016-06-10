/*****************************************************************************
 * main.cpp : example usage of libcloudstorage
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

#include <CloudStorage.h>
#include <jsoncpp/json/json.h>
#include <cassert>
#include <fstream>
#include <iostream>

class Callback : public cloudstorage::ICloudProvider::ICallback {
 public:
  Status userConsentRequired(
      const cloudstorage::ICloudProvider& provider) const {
    std::cout << "required consent at url: \n";
    std::cout << provider.authorizeLibraryUrl() << "\n";
    return Status::WaitForAuthorizationCode;
  }
};

void traverse_drive(cloudstorage::ICloudProvider& drive,
                    cloudstorage::IItem::Pointer f, std::string path) {
  std::cout << path << "\n";
  if (!f->is_directory()) return;
  for (cloudstorage::IItem::Pointer& t : drive.listDirectory(*f)) {
    traverse_drive(drive, std::move(t),
                   path + t->filename() + (t->is_directory() ? "/" : ""));
  }
}

int main(int argc, char** argv) {
  std::string drive_backend = "google";
  if (argc >= 2) drive_backend = argv[1];
  std::string drive_file = drive_backend + ".json";

  cloudstorage::ICloudProvider::Pointer drive =
      cloudstorage::CloudStorage().providerFromFile(drive_file);

  if (drive == nullptr) {
    drive = cloudstorage::CloudStorage().provider(drive_backend);
    if (drive == nullptr) {
      std::cout << "Invalid drive backend.\n";
      return 1;
    }
  }
  if (drive->initialize(std::unique_ptr<Callback>(new Callback))) {
    std::fstream file(drive_file, std::fstream::out);
    file << drive->dump();
    file.close();
    traverse_drive(*drive, drive->rootDirectory(), "/");
  } else {
    std::cout << "Failed to obtain access token.\n";
    return 1;
  }

  return 0;
}
