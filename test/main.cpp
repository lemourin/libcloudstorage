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

#include <ICloudStorage.h>
#include <cassert>
#include <fstream>
#include <iostream>

class Callback : public cloudstorage::ICloudProvider::ICallback {
 public:
  Callback(std::string drive_file) : drive_file_(drive_file) {}

  Status userConsentRequired(
      const cloudstorage::ICloudProvider& provider) override {
    std::cout << "required consent at url: \n";
    std::cout << provider.authorizeLibraryUrl() << "\n";
    return Status::WaitForAuthorizationCode;
  }

  void accepted(const cloudstorage::ICloudProvider& provider) override {
    std::fstream file(drive_file_, std::fstream::out);
    file << provider.token();
  }

  void declined(const cloudstorage::ICloudProvider&) override {
    std::cerr << "access denied ;_;\n";
  }

  void error(const cloudstorage::ICloudProvider&, const std::string&) override {
  }

 private:
  std::string drive_file_;
};

void traverse_drive(cloudstorage::ICloudProvider& drive,
                    cloudstorage::IItem::Pointer f, std::string path) {
  std::cout << path << "\n";
  if (f->type() != cloudstorage::IItem::FileType::Directory) return;
  for (cloudstorage::IItem::Pointer& t :
       drive.listDirectoryAsync(f)->result()) {
    traverse_drive(
        drive, std::move(t),
        path + t->filename() +
            (t->type() == cloudstorage::IItem::FileType::Directory ? "/" : ""));
  }
}

int main(int argc, char** argv) {
  std::string drive_backend = "google";
  if (argc >= 2) drive_backend = argv[1];

  cloudstorage::ICloudProvider::Pointer drive =
      cloudstorage::ICloudStorage::create()->provider(drive_backend);
  if (drive == nullptr) {
    std::cout << "Invalid drive backend.\n";
    return 1;
  }
  std::string drive_file = drive_backend + ".txt";
  std::string token;
  {
    std::fstream file(drive_file, std::fstream::in);
    file >> token;
  }
  drive->initialize({token,
                     std::unique_ptr<Callback>(new Callback(drive_file)),
                     nullptr,
                     nullptr,
                     nullptr,
                     {}});
  traverse_drive(*drive, drive->rootDirectory(), "/");

  return 0;
}
