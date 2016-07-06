/*****************************************************************************
 * Window.h : Window prototypes
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

#ifndef WINDOW_HPP
#define WINDOW_HPP

#include <ICloudProvider.h>
#include <IItem.h>
#include <QMediaObject>
#include <QQuickImageProvider>
#include <QQuickView>
#include "InputDevice.h"

using ItemPointer = cloudstorage::IItem::Pointer;
using ImagePointer = std::shared_ptr<QImage>;

class Window;

class ImageProvider : public QQuickImageProvider {
 public:
  ImageProvider();
  QImage requestImage(const QString&, QSize*, const QSize&);
  void addImage(QString id, ImagePointer);
  bool hasImage(QString id);

 private:
  std::map<QString, ImagePointer> cache_;
};

class ItemModel : public QObject {
 public:
  Q_PROPERTY(QString name READ name CONSTANT)
  Q_PROPERTY(bool directory READ is_directory CONSTANT)
  Q_PROPERTY(QString thumbnail READ thumbnail NOTIFY thumbnailChanged)

  ItemModel(cloudstorage::IItem::Pointer item,
            cloudstorage::ICloudProvider::Pointer, Window*);
  ~ItemModel();

  QString name() const { return item_->filename().c_str(); }
  bool is_directory() const { return item_->is_directory(); }
  cloudstorage::IItem::Pointer item() const { return item_; }
  QString thumbnail() const { return thumbnail_; }

 signals:
  void thumbnailChanged();
  void receivedImage(ImagePointer);

 private:
  QString thumbnail_;
  cloudstorage::IItem::Pointer item_;
  cloudstorage::DownloadFileRequest::Pointer thumbnail_request_;
  Window* window_;

  Q_OBJECT
};

class CloudProviderCallback : public cloudstorage::ICloudProvider::ICallback {
 public:
  CloudProviderCallback(Window*);

  Status userConsentRequired(const cloudstorage::ICloudProvider&);
  void accepted(const cloudstorage::ICloudProvider&);
  void declined(const cloudstorage::ICloudProvider&);
  void error(const cloudstorage::ICloudProvider&, const std::string&);

 private:
  Window* window_;
};

class Window : public QQuickView {
 public:
  Q_PROPERTY(QObject* mediaObject READ mediaObject CONSTANT)

  Window();
  ~Window();

  QObject* mediaObject() { return &media_player_; };
  ImageProvider* imageProvider() { return image_provider_; }

  Q_INVOKABLE void initializeCloud(QString name);
  Q_INVOKABLE void listDirectory();
  Q_INVOKABLE void changeCurrentDirectory(ItemModel* directory);
  Q_INVOKABLE bool goBack();
  Q_INVOKABLE void play(ItemModel* item, QString method);
  Q_INVOKABLE void stop();
  Q_INVOKABLE bool playing();
  Q_INVOKABLE void uploadFile(QString path);

  void onSuccessfullyAuthorized();
  void onAddedItem(cloudstorage::IItem::Pointer);
  void onPlay(cloudstorage::IItem::Pointer);
  void onRunPlayer();
  void onPlayFile(QString filename);
  void onPlayFileFromUrl(QString url);
  void onPausePlayer();
  void onMediaStatusChanged(QMediaPlayer::MediaStatus);

 signals:
  void openBrowser(QString url);
  void closeBrowser();
  void successfullyAuthorized();
  void addedItem(ItemPointer);
  void runPlayer(QString file);
  void runPlayerFromUrl(QString url);
  void cloudChanged();
  void progressChanged(int total, int now);

 private:
  friend class CloudProviderCallback;

  void clearCurrentDirectoryList();

  cloudstorage::ICloudProvider::Pointer cloud_provider_;
  cloudstorage::AuthorizeRequest::Pointer authorization_request_;
  cloudstorage::IItem::Pointer current_directory_;
  std::vector<cloudstorage::IItem::Pointer> directory_stack_;
  cloudstorage::ListDirectoryRequest::Pointer list_directory_request_;
  QObjectList current_directory_list_;
  std::unique_ptr<InputDevice> device_;
  QMediaPlayer media_player_;
  DownloadFileRequest::Pointer download_request_;
  UploadFileRequest::Pointer upload_request_;
  GetItemDataRequest::Pointer item_data_request_;
  ImageProvider* image_provider_;

  Q_OBJECT
};

Q_DECLARE_METATYPE(ItemPointer)
Q_DECLARE_METATYPE(ImagePointer)

#endif  // WINDOW_HPP
