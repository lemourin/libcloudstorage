/*****************************************************************************
 * InputDevice.cpp : InputDevice implementation
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

#include "InputDevice.h"

#include <iostream>

#include "Window.h"

const int MIN_DATA_SIZE = 1000000;

using namespace cloudstorage;

DownloadFileCallback::DownloadFileCallback(InputDevice* p) : device_(p) {}

void DownloadFileCallback::receivedData(const char* data, uint32_t length) {
  std::lock_guard<std::mutex> lock(device_->buffer_mutex_);
  device_->buffer_ += std::string(data, length);
  device_->length_ += length;
  if (!device_->isSequential()) emit device_->readyRead();
}

void DownloadFileCallback::done() {
  std::lock_guard<std::mutex> lock(device_->buffer_mutex_);
  device_->finished_ = true;

  emit device_->runPlayer();

  std::cerr << "[OK] Finished downloading\n";
}

void DownloadFileCallback::error(const std::string& error) {
  std::cerr << "[FAIL] " << error.c_str() << "\n";
}

void DownloadFileCallback::progress(uint32_t total, uint32_t) {
  {
    std::lock_guard<std::mutex> lock(device_->buffer_mutex_);
    device_->total_length_ = total;
  }
  if (device_->isSequential() && device_->bytesAvailable() >= 2 * MIN_DATA_SIZE)
    emit device_->runPlayer();
}

InputDevice::InputDevice(bool streaming)
    : finished_(),
      length_(),
      position_(),
      total_length_(),
      streaming_(streaming) {
  open(QIODevice::ReadOnly);
}

qint64 InputDevice::bytesAvailable() const {
  uint32_t bytes;
  {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    bytes = length_ - position_;
  }
  if (isSequential() && bytes < MIN_DATA_SIZE && !finished_) emit pausePlayer();
  return bytes + QIODevice::bytesAvailable();
}

qint64 InputDevice::size() const {
  std::lock_guard<std::mutex> lock(buffer_mutex_);
  if (isSequential()) return QIODevice::size();
  return total_length_;
}

qint64 InputDevice::readData(char* data, qint64 length) {
  std::lock_guard<std::mutex> lock(buffer_mutex_);
  uint32_t i;
  for (i = 0; i < length; i++)
    if (i + position_ < buffer_.length())
      data[i] = buffer_[i + position_];
    else
      break;
  position_ += i;
  return i;
}

bool InputDevice::seek(qint64 position) {
  QIODevice::seek(position);
  position_ = position;
  return true;
}

DownloadToFileCallback::DownloadToFileCallback(Window* window,
                                               std::string filename)
    : window_(window),
      file_(filename, std::ios_base::out | std::ios_base::binary),
      filename_(filename) {}

void DownloadToFileCallback::receivedData(const char* data, uint32_t length) {
  file_.write(data, length);
}

void DownloadToFileCallback::done() {
  file_.close();
  emit window_->runPlayer(filename_.c_str());
  std::cerr << "[OK] Finished download.\n";
}

void DownloadToFileCallback::error(const std::string&) {}

void DownloadToFileCallback::progress(uint32_t, uint32_t) {}

UploadFileCallback::UploadFileCallback(Window* window, QUrl url)
    : window_(window), file_(url.toLocalFile()) {
  file_.open(QFile::ReadOnly);
}

void UploadFileCallback::reset() {
  std::cerr << "[FAIL] Retransmission needed\n";
}

uint32_t UploadFileCallback::putData(char* data, uint32_t maxlength) {
  return static_cast<uint32_t>(file_.read(data, maxlength));
}

void UploadFileCallback::done() {
  std::cerr << "[OK] Successfuly uploaded\n";
  emit window_->cloudChanged();
  emit window_->progressChanged(0, 0);
}

void UploadFileCallback::error(const std::string& description) {
  std::cerr << "[FAIL] Upload: " << description << "\n";
  emit window_->progressChanged(0, 0);
}

void UploadFileCallback::progress(uint32_t total, uint32_t now) {
  emit window_->progressChanged(total, now);
}
