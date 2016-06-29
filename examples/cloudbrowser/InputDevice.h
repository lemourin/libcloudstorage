/*****************************************************************************
 * InputDevice.h : InputDevice prototypes
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

#ifndef INPUTDEVICE_HPP
#define INPUTDEVICE_HPP

#include <ICloudStorage.h>
#include <QMediaPlayer>
#include <QObject>
#include <queue>

using namespace cloudstorage;

class InputDevice;

class DownloadFileCallback : public DownloadFileRequest::ICallback {
 public:
  DownloadFileCallback(InputDevice *p);

  void reset();
  void receivedData(const char *data, uint length);
  void done();

 private:
  InputDevice *device_;
};

class InputDevice : public QIODevice,
                    public std::enable_shared_from_this<InputDevice> {
 public:
  InputDevice();

  qint64 bytesAvailable() const;
  bool isSequential() const { return true; }

  qint64 readData(char *data, qint64 length);

  qint64 writeData(const char *, qint64) { return 0; }

 signals:
  void runPlayer() const;
  void pausePlayer() const;

 private:
  friend class DownloadFileCallback;

  QMediaPlayer *player_;
  mutable std::mutex queue_mutex_;
  std::queue<char> queue_;
  ICloudProvider::Pointer cloud_provider_;
  GetItemRequest::Pointer item_request_;
  DownloadFileRequest::Pointer download_request_;
  AuthorizeRequest::Pointer authorize_request_;
  bool finished_;

  Q_OBJECT
};

#endif  // INPUTDEVICE_HPP
