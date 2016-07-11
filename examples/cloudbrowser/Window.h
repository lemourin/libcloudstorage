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

#ifndef WINDOW_H
#define WINDOW_H

#include <ICloudProvider.h>
#include <IItem.h>
#include <QQuickImageProvider>
#include <QQuickView>
#include <future>
#include <vlcpp/vlc.hpp>

#include "Callback.h"

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

  QString name() const { return item_->filename().c_str(); }
  bool is_directory() const { return item_->is_directory(); }
  cloudstorage::IItem::Pointer item() const { return item_; }
  QString thumbnail() const { return thumbnail_; }

 signals:
  void thumbnailChanged();
  void receivedImage(ImagePointer);

 private:
  friend class Window;

  QString thumbnail_;
  cloudstorage::IItem::Pointer item_;
  cloudstorage::ICloudProvider::DownloadFileRequest::Pointer thumbnail_request_;
  Window* window_;

  Q_OBJECT
};

class Window : public QQuickView {
 public:
  Window();
  ~Window();

  ImageProvider* imageProvider() { return image_provider_; }

  Q_INVOKABLE void initializeCloud(QString name);
  Q_INVOKABLE void listDirectory();
  Q_INVOKABLE void changeCurrentDirectory(ItemModel* directory);
  Q_INVOKABLE bool goBack();
  Q_INVOKABLE void play(ItemModel* item);
  Q_INVOKABLE void stop();
  Q_INVOKABLE void uploadFile(QString path);
  Q_INVOKABLE void downloadFile(ItemModel*, QUrl path);

  void onSuccessfullyAuthorized();
  void onAddedItem(cloudstorage::IItem::Pointer);
  void onPlayFile(QString filename);
  void onPlayFileFromUrl(QString url);

 signals:
  void openBrowser(QString url);
  void closeBrowser();
  void successfullyAuthorized();
  void addedItem(ItemPointer);
  void runPlayer(QString file);
  void runPlayerFromUrl(QString url);
  void cloudChanged();
  void uploadProgressChanged(int total, int now);
  void downloadProgressChanged(int total, int now);
  void showPlayer();
  void hidePlayer();
  void runListDirectory();
  void runClearDirectory();

 protected:
  void keyPressEvent(QKeyEvent*);

 private:
  friend class CloudProviderCallback;

  void clearCurrentDirectoryList();
  void saveCloudAccessToken();
  void initializeMediaPlayer();
  void startDirectoryClear(std::function<void()>);

  ICloudProvider::Hints fromQMap(const QMap<QString, QVariant>&) const;
  QMap<QString, QVariant> toQMap(const ICloudProvider::Hints&) const;

  ICloudProvider::Pointer cloud_provider_;
  IItem::Pointer current_directory_;
  std::vector<cloudstorage::IItem::Pointer> directory_stack_;
  ICloudProvider::ListDirectoryRequest::Pointer list_directory_request_;
  ICloudProvider::DownloadFileRequest::Pointer download_request_;
  ICloudProvider::UploadFileRequest::Pointer upload_request_;
  ICloudProvider::GetItemDataRequest::Pointer item_data_request_;
  ImageProvider* image_provider_;
  VLC::Instance vlc_instance_;
  VLC::MediaPlayer media_player_;
  std::future<void> clear_directory_;

  Q_OBJECT
};

Q_DECLARE_METATYPE(ItemPointer)
Q_DECLARE_METATYPE(ImagePointer)

#endif  // WINDOW_H
