/*****************************************************************************
 * Callback.cpp : Callback implementation
 *
 *****************************************************************************
 * Copyright (C) 2016 VideoLAN
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

#include "Callback.h"

#include <QDir>
#include <QSettings>
#include <iostream>

#include "Utility/Utility.h"
#include "Window.h"

using cloudstorage::util::make_unique;

DownloadFileCallback::DownloadFileCallback(Window* window, std::string filename)
    : window_(window), file_(filename.c_str()), filename_(filename) {
  file_.open(QFile::WriteOnly);
}

void DownloadFileCallback::receivedData(const char* data, uint32_t length) {
  file_.write(data, length);
}

void DownloadFileCallback::done(EitherError<void> e) {
  auto lock = window_->stream_lock();
  if (e.left()) {
    std::cerr << "[FAIL] Download: " << e.left()->code_ << " "
              << e.left()->description_ << "\n";
  } else {
    std::cerr << "[OK] Finished download.\n";
  }
  file_.close();
  emit window_->downloadProgressChanged(0, 0);
}

void DownloadFileCallback::progress(uint32_t total, uint32_t now) {
  emit window_->downloadProgressChanged(total, now);
}

UploadFileCallback::UploadFileCallback(Window* window, QUrl url)
    : window_(window), file_(url.toLocalFile()) {
  file_.open(QFile::ReadOnly);
}

void UploadFileCallback::reset() {
  {
    auto lock = window_->stream_lock();
    std::cerr << "[DIAG] Starting transmission\n";
  }
  file_.reset();
}

uint32_t UploadFileCallback::putData(char* data, uint32_t maxlength) {
  return file_.read(data, maxlength);
}

uint64_t UploadFileCallback::size() { return file_.size(); }

void UploadFileCallback::done(EitherError<IItem> e) {
  auto lock = window_->stream_lock();
  if (e.left()) {
    std::cerr << "[FAIL] Upload: " << e.left()->code_ << " "
              << e.left()->description_ << "\n";
  } else {
    std::cerr << "[OK] Successfully uploaded " << e.right()->filename() << "\n";
  }
  emit window_->uploadProgressChanged(0, 0);
}

void UploadFileCallback::progress(uint32_t total, uint32_t now) {
  emit window_->uploadProgressChanged(total, now);
}

CloudProviderCallback::CloudProviderCallback(Window* w) : window_(w) {}

ICloudProvider::IAuthCallback::Status
CloudProviderCallback::userConsentRequired(const ICloudProvider& p) {
  auto lock = window_->stream_lock();
  std::cerr << "[DIAG] User consent required: " << p.authorizeLibraryUrl()
            << "\n";
  emit window_->consentRequired(p.name().c_str());
  emit window_->openBrowser(p.authorizeLibraryUrl().c_str());
  return Status::WaitForAuthorizationCode;
}

void CloudProviderCallback::done(const ICloudProvider& drive,
                                 EitherError<void> e) {
  if (e.left()) {
    auto lock = window_->stream_lock();
    std::cerr << "[FAIL] Authorize error: " << e.left()->code_ << " "
              << e.left()->description_ << "\n";
    emit window_->closeBrowser();
  } else {
    QSettings settings;
    settings.setValue(drive.name().c_str(), drive.token().c_str());
    emit window_->closeBrowser();
    emit window_->successfullyAuthorized(drive.name().c_str());
  }
}

ListDirectoryCallback::ListDirectoryCallback(Window* w) : window_(w) {}

void ListDirectoryCallback::receivedItem(IItem::Pointer item) {
  emit window_->addedItem(item);
}

void ListDirectoryCallback::done(EitherError<std::vector<IItem::Pointer>> e) {
  if (e.left()) {
    auto lock = window_->stream_lock();
    std::cerr << "[FAIL] ListDirectory: " << e.left()->code_ << " "
              << e.left()->description_ << "\n";
    emit window_->closeBrowser();
  }
}

DownloadThumbnailCallback::DownloadThumbnailCallback(ItemModel* i) : item_(i) {}

void DownloadThumbnailCallback::receivedData(const char* data,
                                             uint32_t length) {
  data_ += std::string(data, data + length);
}

void DownloadThumbnailCallback::done(EitherError<void> e) {
  if (e.left()) {
    emit item_->failedImage();
    return;
  }
  QFile file(QDir::tempPath() + "/" +
             Window::escapeFileName(item_->item()->filename()).c_str() +
             ".thumbnail");
  file.open(QFile::WriteOnly);
  file.write(data_.data(), data_.length());
  emit item_->receivedImage();
}

void DownloadThumbnailCallback::progress(uint32_t, uint32_t) {}
