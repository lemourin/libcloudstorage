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
#include <QFile>
#include <fstream>
#include <queue>

using namespace cloudstorage;

class InputDevice;
class Window;

class DownloadFileCallback : public DownloadFileRequest::ICallback {
 public:
  DownloadFileCallback(InputDevice *p);

  void receivedData(const char *data, uint32_t length);
  void done();
  void error(const std::string &);
  void progress(uint32_t total, uint32_t now);

 private:
  InputDevice *device_;
};

class DownloadToFileCallback : public DownloadFileRequest::ICallback {
 public:
  DownloadToFileCallback(Window *, std::string filename);

  void receivedData(const char *data, uint32_t length);
  void done();
  void error(const std::string &);
  void progress(uint32_t total, uint32_t now);

 private:
  Window *window_;
  std::fstream file_;
  std::string filename_;
};

class UploadFileCallback : public UploadFileRequest::ICallback {
 public:
  UploadFileCallback(Window*, QUrl url);

  void reset();
  uint32_t putData(char *data, uint32_t maxlength);
  void done();
  void error(const std::string &description);
  void progress(uint32_t total, uint32_t now);

 private:
  Window* window_;
  std::fstream file_;
};

class InputDevice : public QIODevice,
                    public std::enable_shared_from_this<InputDevice> {
 public:
  InputDevice(bool streaming);

  qint64 bytesAvailable() const;
  qint64 size() const;
  bool isSequential() const { return streaming_; }

  qint64 readData(char *data, qint64 length);

  qint64 writeData(const char *, qint64) { return 0; }

  bool seek(qint64);

 signals:
  void runPlayer() const;
  void pausePlayer() const;

 private:
  friend class DownloadFileCallback;

  QMediaPlayer *player_;
  mutable std::mutex buffer_mutex_;
  ICloudProvider::Pointer cloud_provider_;
  GetItemRequest::Pointer item_request_;
  DownloadFileRequest::Pointer download_request_;
  AuthorizeRequest::Pointer authorize_request_;
  bool finished_;
  std::string buffer_;
  uint32_t length_;
  uint32_t position_;
  uint32_t total_length_;
  bool streaming_;

  Q_OBJECT
};

#endif  // INPUTDEVICE_HPP
