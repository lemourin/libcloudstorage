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

#include <QSettings>
#include <iostream>

#include "Window.h"

using namespace cloudstorage;

DownloadFileCallback::DownloadFileCallback(Window* window, std::string filename)
    : window_(window),
      file_(filename, std::ios_base::out | std::ios_base::binary),
      filename_(filename) {}

void DownloadFileCallback::receivedData(const char* data, uint32_t length) {
  file_.write(data, length);
}

void DownloadFileCallback::done() {
  file_.close();
  emit window_->downloadProgressChanged(0, 0);
  std::cerr << "[OK] Finished download.\n";
}

void DownloadFileCallback::error(const std::string& desc) {
  std::cerr << "[FAIL] Download: " << desc << "\n";
  emit window_->downloadProgressChanged(0, 0);
}

void DownloadFileCallback::progress(uint32_t total, uint32_t now) {
  emit window_->downloadProgressChanged(total, now);
}

UploadFileCallback::UploadFileCallback(Window* window, QUrl url)
    : window_(window),
      file_(url.toLocalFile().toStdString(),
            std::ios_base::in | std::ios_base::binary) {}

void UploadFileCallback::reset() {
  std::cerr << "[FAIL] Retransmission needed\n";
  file_.seekg(std::ios::beg);
}

uint32_t UploadFileCallback::putData(char* data, uint32_t maxlength) {
  file_.read(data, maxlength);
  return file_.gcount();
}

void UploadFileCallback::done() {
  std::cerr << "[OK] Successfuly uploaded\n";
  emit window_->uploadProgressChanged(0, 0);
}

void UploadFileCallback::error(const std::string& description) {
  std::cerr << "[FAIL] Upload: " << description << "\n";
  emit window_->uploadProgressChanged(0, 0);
}

void UploadFileCallback::progress(uint32_t total, uint32_t now) {
  emit window_->uploadProgressChanged(total, now);
}

CloudProviderCallback::CloudProviderCallback(Window* w) : window_(w) {}

ICloudProvider::ICallback::Status CloudProviderCallback::userConsentRequired(
    const ICloudProvider& p) {
  std::cerr << "[DIAG] User consent required: " << p.authorizeLibraryUrl()
            << "\n";
  emit window_->openBrowser(p.authorizeLibraryUrl().c_str());
  return Status::WaitForAuthorizationCode;
}

void CloudProviderCallback::accepted(const ICloudProvider& drive) {
  QSettings settings;
  settings.setValue(drive.name().c_str(), drive.token().c_str());
  emit window_->closeBrowser();
  emit window_->successfullyAuthorized();
}

void CloudProviderCallback::declined(const ICloudProvider&) {
  emit window_->closeBrowser();
}

void CloudProviderCallback::error(const ICloudProvider&,
                                  const std::string& desc) {
  std::cerr << "[FAIL] " << desc.c_str() << "\n";
}
