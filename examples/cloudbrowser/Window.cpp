/*****************************************************************************
 * Window.cpp : Window implementation
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

#include "Window.h"

#include <QDebug>
#include <QDir>
#include <QMetaEnum>
#include <QMetaObject>
#include <QQmlContext>
#include <QQuickItem>
#include <QSettings>
#include <iostream>

#include "MockProvider.h"

using namespace cloudstorage;

Window::Window()
    : image_provider_(new ImageProvider),
      vlc_instance_(0, nullptr),
      media_player_(vlc_instance_) {
  qRegisterMetaType<ItemPointer>();
  qRegisterMetaType<ImagePointer>();

  setClearBeforeRendering(false);
  initializeMediaPlayer();

  QStringList clouds;
  for (auto p : ICloudStorage::create()->providers())
    clouds.append(p->name().c_str());
  clouds.append("mock");
  rootContext()->setContextProperty("cloudModel", clouds);
  rootContext()->setContextProperty("window", this);
  rootContext()->setContextProperty("directoryModel", &directory_model_);
  engine()->addImageProvider("provider", imageProvider());

  setSource(QUrl("qrc:/main.qml"));
  setResizeMode(SizeRootObjectToView);

  connect(this, &Window::successfullyAuthorized, this,
          &Window::onSuccessfullyAuthorized);
  connect(this, &Window::addedItem, this, &Window::onAddedItem);
  connect(this, &Window::runPlayer, this, &Window::onPlayFile);
  connect(this, &Window::runPlayerFromUrl, this, &Window::onPlayFileFromUrl);
  connect(this, &Window::cloudChanged, this, &Window::listDirectory);
  connect(this, &Window::showPlayer, this,
          [this]() { contentItem()->setVisible(false); }, Qt::QueuedConnection);
  connect(this, &Window::hidePlayer, this,
          [this]() {
            media_player_.stop();
            contentItem()->setVisible(true);
          },
          Qt::QueuedConnection);
  connect(this, &Window::runListDirectory, this, [this]() { listDirectory(); },
          Qt::QueuedConnection);
  connect(this, &Window::runClearDirectory, this,
          [this]() { clearCurrentDirectoryList(); }, Qt::QueuedConnection);
}

Window::~Window() {
  if (clear_directory_.valid()) clear_directory_.get();
  clearCurrentDirectoryList();
  saveCloudAccessToken();
}

void Window::initializeCloud(QString name) {
  saveCloudAccessToken();

  QSettings settings;
  if (name != "mock")
    cloud_provider_ = ICloudStorage::create()->provider(name.toStdString());
  else
    cloud_provider_ = make_unique<MockProvider>();
  std::cerr << "[DIAG] Trying to authorize with "
            << settings.value(name).toString().toStdString() << std::endl;
  ICloudProvider::Hints hints =
      fromQMap(settings.value(name + "_hints").toMap());
  cloud_provider_->initialize(settings.value(name).toString().toStdString(),
                              make_unique<CloudProviderCallback>(this), hints);
  current_directory_ = cloud_provider_->rootDirectory();
  emit runListDirectory();
}

void Window::listDirectory() {
  clearCurrentDirectoryList();
  list_directory_request_ = cloud_provider_->listDirectoryAsync(
      current_directory_, make_unique<ListDirectoryCallback>(this));
}

void Window::changeCurrentDirectory(int directory_id) {
  auto directory = directory_model_.get(directory_id);
  directory_stack_.push_back(current_directory_);
  current_directory_ = directory->item();

  list_directory_request_ = nullptr;
  startDirectoryClear([this]() { emit runListDirectory(); });
}

void Window::onSuccessfullyAuthorized() {
  std::cerr << "[OK] Successfully authorized "
            << cloud_provider_->name().c_str() << "\n";
}

void Window::onAddedItem(IItem::Pointer i) {
  directory_model_.addItem(i, this);
}

void Window::onPlayFile(QString filename) {
  VLC::Media media(vlc_instance_, QDir::currentPath().toStdString() + "/" +
                                      filename.toStdString(),
                   VLC::Media::FromPath);
  media_player_.setMedia(media);
  media_player_.play();
}

void Window::onPlayFileFromUrl(QString url) {
  std::cerr << "[DIAG] Playing url " << url.toStdString() << "\n";
  VLC::Media media(vlc_instance_, url.toStdString(), VLC::Media::FromLocation);
  media_player_.setMedia(media);
  media_player_.play();
}

void Window::keyPressEvent(QKeyEvent* e) {
  QQuickView::keyPressEvent(e);
  if (e->isAccepted()) return;
  if (e->key() == Qt::Key_Q)
    stop();
  else if (e->key() == Qt::Key_P)
    media_player_.pause();
}

void Window::clearCurrentDirectoryList() { directory_model_.clear(); }

void Window::saveCloudAccessToken() {
  if (cloud_provider_) {
    clearCurrentDirectoryList();
    QSettings settings;
    settings.setValue((cloud_provider_->name() + "_hints").c_str(),
                      toQMap(cloud_provider_->hints()));
  }
}

void Window::initializeMediaPlayer() {
#ifdef Q_OS_LINUX
  media_player_.setXwindow(winId());
#elif defined Q_OS_WIN
  media_player_.setHwnd(reinterpret_cast<void*>(winId()));
#elif defined Q_OS_DARWIN
  media_player_.setNsobject(reinterpret_cast<void*>(winId()));
#endif

  media_player_.eventManager().onPlaying([this]() {
    if (media_player_.videoTrackCount() > 0) emit showPlayer();
  });
  media_player_.eventManager().onStopped([this]() { emit hidePlayer(); });
}

void Window::startDirectoryClear(std::function<void()> f) {
  if (list_directory_request_) list_directory_request_->cancel();
  clear_directory_ = std::async(std::launch::async, [this, f]() {
    for (int i = 0; i < directory_model_.rowCount(); i++) {
      auto model = directory_model_.get(i);
      if (model->thumbnail_request_) model->thumbnail_request_->cancel();
    }
    f();
  });
}

ICloudProvider::Hints Window::fromQMap(
    const QMap<QString, QVariant>& map) const {
  ICloudProvider::Hints result;
  for (auto it = map.begin(); it != map.end(); it++)
    result[it.key().toStdString()] = it.value().toString().toStdString();
  return result;
}

QMap<QString, QVariant> Window::toQMap(const ICloudProvider::Hints& map) const {
  QMap<QString, QVariant> result;
  for (auto it = map.begin(); it != map.end(); it++)
    result[it->first.c_str()] = it->second.c_str();
  return result;
}

bool Window::goBack() {
  if (directory_stack_.empty()) {
    startDirectoryClear([this]() { emit runClearDirectory(); });
    return false;
  }
  current_directory_ = directory_stack_.back();
  directory_stack_.pop_back();
  startDirectoryClear([this]() { emit runListDirectory(); });
  return true;
}

void Window::play(int item_id) {
  ItemModel* item = directory_model_.get(item_id);
  stop();
  item_data_request_ = cloud_provider_->getItemDataAsync(
      item->item()->id(),
      [this](IItem::Pointer i) { emit runPlayerFromUrl(i->url().c_str()); });
}

void Window::stop() {
  std::cerr << "[DIAG] Trying to stop player\n";
  media_player_.stop();
  std::cerr << "[DIAG] Stopped\n";
  download_request_ = nullptr;
}

void Window::uploadFile(QString path) {
  std::cerr << "[DIAG] Uploading file " << path.toStdString() << "\n";
  QUrl url = path;
  upload_request_ = cloud_provider_->uploadFileAsync(
      current_directory_, url.fileName().toStdString(),
      make_unique<UploadFileCallback>(this, url));
}

void Window::downloadFile(int item_id, QUrl path) {
  ItemModel* i = directory_model_.get(item_id);
  download_request_ = cloud_provider_->downloadFileAsync(
      i->item(),
      make_unique<DownloadFileCallback>(this, path.toLocalFile().toStdString() +
                                                  "/" + i->item()->filename()));
  std::cerr << "[DIAG] Downloading file " << path.toLocalFile().toStdString()
            << "\n";
}

ItemModel::ItemModel(IItem::Pointer item, ICloudProvider::Pointer p, Window* w)
    : item_(item), window_(w) {
  QString id = (p->name() + "/" + item_->id()).c_str();
  connect(this, &ItemModel::receivedImage, [this, id](ImagePointer image) {
    window_->imageProvider()->addImage(id, std::move(image));
    thumbnail_ = "image://provider//" + id;
    emit thumbnailChanged();
  });

  if (window_->imageProvider()->hasImage(id))
    thumbnail_ = "image://provider//" + id;
  else
    thumbnail_request_ = p->getThumbnailAsync(
        item, make_unique<DownloadThumbnailCallback>(this));
}

ImageProvider::ImageProvider() : QQuickImageProvider(Image) {}

QImage ImageProvider::requestImage(const QString& id, QSize* size,
                                   const QSize& requested_size) {
  if (cache_.find(id) == cache_.end()) return QImage();
  QImage img = *cache_[id];
  if (requested_size.isValid())
    img = img.scaled(requested_size.width(), requested_size.height());
  *size = requested_size;
  return img;
}

void ImageProvider::addImage(QString id, ImagePointer img) {
  cache_["/" + id] = std::move(img);
}

bool ImageProvider::hasImage(QString id) {
  return cache_.find("/" + id) != cache_.end();
}

int DirectoryModel::rowCount(const QModelIndex&) const { return list_.size(); }

QVariant DirectoryModel::data(const QModelIndex& id, int) const {
  QVariantMap dict;
  dict["thumbnail"] = list_[id.row()]->thumbnail();
  dict["name"] = list_[id.row()]->item()->filename().c_str();
  dict["is_directory"] =
      list_[id.row()]->item()->type() == IItem::FileType::Directory;
  return dict;
}

void DirectoryModel::addItem(IItem::Pointer item, Window* w) {
  beginInsertRows(QModelIndex(), rowCount(), rowCount());
  auto model = make_unique<ItemModel>(item, w->cloud_provider_, w);
  list_.push_back(std::move(model));
  endInsertRows();
  int idx = rowCount() - 1;
  connect(list_.back().get(), &ItemModel::thumbnailChanged, this,
          [this, idx]() { emit dataChanged(index(idx, 0), index(idx, 0)); },
          Qt::QueuedConnection);
}

void DirectoryModel::clear() {
  beginRemoveRows(QModelIndex(), 0, rowCount() - 1);
  list_.clear();
  endRemoveRows();
}
