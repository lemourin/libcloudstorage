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
#include <cassert>
#include <fstream>
#include <iostream>

class Callback : public cloudstorage::ICloudProvider::ICallback {
 public:
  Status userConsentRequired(const cloudstorage::ICloudProvider& provider) {
    std::cout << "required consent at url: \n";
    std::cout << provider.authorizeLibraryUrl() << "\n";
    return Status::WaitForAuthorizationCode;
  }
};

void traverse_drive(cloudstorage::ICloudProvider& drive,
                    cloudstorage::IItem::Pointer f, std::string path) {
  std::cout << path << "\n";
  if (!f->is_directory()) return;
  for (cloudstorage::IItem::Pointer& t :
       drive.listDirectoryAsync(f, nullptr)->result()) {
    traverse_drive(drive, std::move(t),
                   path + t->filename() + (t->is_directory() ? "/" : ""));
  }
}

int main(int argc, char** argv) {
  std::string drive_backend = "google";
  if (argc >= 2) drive_backend = argv[1];

  cloudstorage::ICloudProvider::Pointer drive =
      cloudstorage::CloudStorage().provider(drive_backend);
  if (drive == nullptr) {
    std::cout << "Invalid drive backend.\n";
    return 1;
  }
  std::string drive_file = drive_backend + ".json";
  std::string token;
  {
    std::fstream file(drive_file, std::fstream::in);
    file >> token;
  }
  if (!drive->initialize(token, std::unique_ptr<Callback>(new Callback))
           ->result()) {
    std::cout << "Failed to authorize.";
    return 1;
  } else {
    {
      std::fstream file(drive_file, std::fstream::out);
      file << drive->token();
    }
    traverse_drive(*drive, drive->rootDirectory(), "/");
  }

  return 0;
}
