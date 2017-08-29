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
#include <QAbstractListModel>
#include <QQuickView>
#include <future>
#include <unordered_set>

#include "Callback.h"
#include "MediaPlayer.h"

using ItemPointer = cloudstorage::IItem::Pointer;
using cloudstorage::ICloudProvider;
using cloudstorage::ICloudStorage;

class Window;

class ItemModel : public QObject {
 public:
  using Pointer = std::unique_ptr<ItemModel>;

  ItemModel(IItem::Pointer item, std::shared_ptr<ICloudProvider>, Window*);
  ~ItemModel();

  QString name() const { return item_->filename().c_str(); }
  cloudstorage::IItem::Pointer item() const { return item_; }
  QString thumbnail() const { return thumbnail_; }
  void fetchThumbnail();

 signals:
  void thumbnailChanged();
  void receivedImage();
  void failedImage();

 private:
  friend class Window;
  friend class DownloadThumbnailCallback;

  struct ThreadInfo {
    std::mutex lock_;
    bool nuked_ = false;
  };

  QString thumbnail_;
  IItem::Pointer item_;
  ICloudProvider::DownloadFileRequest::Pointer thumbnail_request_;
  std::shared_ptr<ICloudProvider> provider_;
  std::thread thumbnail_thread_;
  std::shared_ptr<ThreadInfo> thread_info_;
  Window* window_;

  Q_OBJECT
};

class DirectoryModel : public QAbstractListModel {
 public:
  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int) const override;

  void addItem(IItem::Pointer, Window* w);
  ItemModel* get(int id) const;

  void clear();

  static QString typeToIcon(IItem::FileType);

 private:
  std::vector<ItemModel::Pointer> list_;
};

class Window : public QQuickView {
 public:
  Q_PROPERTY(QString movedItem READ movedItem NOTIFY movedItemChanged)
  Q_PROPERTY(QString currentMedia READ currentMedia NOTIFY currentMediaChanged)

  Window(MediaPlayer* player_widget);
  ~Window();

  Q_INVOKABLE void initializeCloud(QString name);
  Q_INVOKABLE void listDirectory();
  Q_INVOKABLE void changeCurrentDirectory(int directory);
  Q_INVOKABLE bool goBack();
  Q_INVOKABLE void play(int item);
  Q_INVOKABLE void stop();
  Q_INVOKABLE void uploadFile(QString path);
  Q_INVOKABLE void downloadFile(int, QUrl path);
  Q_INVOKABLE void deleteItem(int item);
  Q_INVOKABLE void createDirectory(QString name);
  Q_INVOKABLE void markMovedItem(int item);
  Q_INVOKABLE void renameItem(int, QString);

  void onSuccessfullyAuthorized(QString);
  void onAddedItem(ItemPointer);
  void onPlayFileFromUrl(QString url);

  std::unique_lock<std::mutex> stream_lock() const {
    return std::unique_lock<std::mutex>(stream_mutex_);
  }
  MediaPlayer* media_player() const { return media_player_; }
  QString movedItem() const;

  QString currentMedia() const { return current_media_; }
  void setCurrentMedia(QString m);

  static std::string escapeFileName(std::string);

 signals:
  void consentRequired(QString cloud);
  void openBrowser(QString url);
  void closeBrowser();
  void successfullyAuthorized(QString);
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
  void playNext();
  void currentItemChanged(int index);
  void movedItemChanged();
  void currentMediaChanged();
  void playQmlPlayer();
  void stopQmlPlayer();
  void pauseQmlPlayer();
  void showQmlPlayer();
  void hideQmlPlayer();
  void showWidgetPlayer();
  void hideWidgetPlayer();

 protected:
  void keyPressEvent(QKeyEvent* e) override;

 private:
  friend class CloudProviderCallback;
  friend class DirectoryModel;

  void clearCurrentDirectoryList();
  void saveCloudAccessToken();
  void startDirectoryClear(std::function<void()>);
  void cancelRequests();

  ICloudProvider::Hints fromJson(const QJsonObject&) const;
  QJsonObject toJson(const ICloudProvider::Hints&) const;

  std::shared_ptr<ICloudProvider> cloud_provider_;
  IItem::Pointer current_directory_;
  std::vector<cloudstorage::IItem::Pointer> directory_stack_;
  ICloudProvider::ListDirectoryRequest::Pointer list_directory_request_;
  ICloudProvider::DownloadFileRequest::Pointer download_request_;
  ICloudProvider::UploadFileRequest::Pointer upload_request_;
  ICloudProvider::GetItemDataRequest::Pointer item_data_request_;
  ICloudProvider::DeleteItemRequest::Pointer delete_item_request_;
  ICloudProvider::CreateDirectoryRequest::Pointer create_directory_request_;
  ICloudProvider::MoveItemRequest::Pointer move_item_request_;
  ICloudProvider::RenameItemRequest::Pointer rename_item_request_;
  IItem::Pointer moved_file_;
  std::future<void> clear_directory_;
  DirectoryModel directory_model_;
  mutable std::mutex stream_mutex_;
  int last_played_;
  MediaPlayer* media_player_;
  QString current_media_;
  std::unordered_map<std::string, std::shared_ptr<ICloudProvider>>
      initialized_clouds_;
  std::unordered_set<std::string> unauthorized_clouds_;

  Q_OBJECT
};

Q_DECLARE_METATYPE(ItemPointer)

#endif  // WINDOW_H
