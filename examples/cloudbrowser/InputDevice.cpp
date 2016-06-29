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

const int MIN_DATA_SIZE = 1000000;

using namespace cloudstorage;

DownloadFileCallback::DownloadFileCallback(InputDevice* p) : device_(p) {}

void DownloadFileCallback::reset() {}

void DownloadFileCallback::receivedData(const char* data, uint length) {
  std::lock_guard<std::mutex> lock(device_->queue_mutex_);
  for (uint i = 0; i < length; i++) device_->queue_.push(data[i]);
  if (device_->queue_.size() >= 2 * MIN_DATA_SIZE) emit device_->runPlayer();
}

void DownloadFileCallback::done() {
  std::lock_guard<std::mutex> lock(device_->queue_mutex_);
  device_->finished_ = true;
  emit device_->runPlayer();
}

InputDevice::InputDevice() : finished_() { open(QIODevice::ReadOnly); }

qint64 InputDevice::bytesAvailable() const {
  std::lock_guard<std::mutex> lock(queue_mutex_);
  if (queue_.size() < MIN_DATA_SIZE && !finished_)
    emit pausePlayer();
  else
    emit runPlayer();
  return queue_.size() + QIODevice::bytesAvailable();
}

qint64 InputDevice::readData(char* data, qint64 length) {
  std::lock_guard<std::mutex> lock(queue_mutex_);
  int i;
  for (i = 0; i < length; i++)
    if (queue_.empty()) {
      break;
    } else {
      data[i] = queue_.front();
      queue_.pop();
    }
  return i;
}
