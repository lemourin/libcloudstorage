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

using namespace cloudstorage;

namespace {

template <typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

class ListDirectoryCallback : public ListDirectoryRequest::ICallback {
 public:
  ListDirectoryCallback(Window* w) : window_(w) {}

  void receivedItem(IItem::Pointer item) { emit window_->addedItem(item); }

  void done(const std::vector<IItem::Pointer>&) {}

  void error(const std::string& str) { std::cerr << "[FAIL] " << str << "\n"; }

 private:
  Window* window_;
};

class DownloadThumbnailCallback : public DownloadFileRequest::ICallback {
 public:
  DownloadThumbnailCallback(ItemModel* i) : item_(i) {}

  void receivedData(const char* data, uint32_t length) {
    data_ += std::string(data, data + length);
  }

  void done() {
    ImagePointer image = make_unique<QImage>();
    if (image->loadFromData(reinterpret_cast<const uchar*>(data_.data()),
                            data_.length()))
      emit item_->receivedImage(std::move(image));
  }

  void error(const std::string& error) {
    std::cerr << "[FAIL] " << error << "\n";
  }

  void progress(uint32_t, uint32_t) {}

 private:
  ItemModel* item_;
  std::string data_;
};

}  // namespace

Window::Window()
    : image_provider_(new ImageProvider),
      vlc_instance_(0, nullptr),
      media_player_(vlc_instance_) {
  qRegisterMetaType<ItemPointer>();
  qRegisterMetaType<ImagePointer>();
  qmlRegisterType<ItemModel>();

  setClearBeforeRendering(false);

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
  media_player_.eventManager().onEndReached([this]() { emit hidePlayer(); });

  QStringList clouds;
  for (auto p : ICloudStorage::create()->providers())
    clouds.append(p->name().c_str());
  rootContext()->setContextProperty("cloudModel", clouds);
  rootContext()->setContextProperty("window", this);
  rootContext()->setContextProperty("directoryModel", nullptr);
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
          [this]() { contentItem()->setVisible(true); }, Qt::QueuedConnection);
}

Window::~Window() { clearCurrentDirectoryList(); }

void Window::initializeCloud(QString name) {
  QSettings settings;
  cloud_provider_ = ICloudStorage::create()->provider(name.toStdString());
  std::cerr << "[DIAG] Trying to authorize with "
            << settings.value(name).toString().toStdString() << std::endl;
  authorization_request_ =
      cloud_provider_->initialize(settings.value(name).toString().toStdString(),
                                  make_unique<CloudProviderCallback>(this));
}

void Window::listDirectory() {
  clearCurrentDirectoryList();
  list_directory_request_ = cloud_provider_->listDirectoryAsync(
      current_directory_, make_unique<ListDirectoryCallback>(this));
}

void Window::changeCurrentDirectory(ItemModel* directory) {
  directory_stack_.push_back(current_directory_);
  current_directory_ = directory->item();
  listDirectory();
}

void Window::onSuccessfullyAuthorized() {
  std::cerr << "[OK] Successfully authorized "
            << cloud_provider_->name().c_str() << "\n";
  current_directory_ = cloud_provider_->rootDirectory();
  listDirectory();
}

void Window::onAddedItem(IItem::Pointer i) {
  QObjectList object_list =
      rootContext()->contextProperty("directoryModel").value<QObjectList>();
  ItemModel* model = new ItemModel(i, cloud_provider_, this);
  model->setParent(this);
  object_list.append(model);
  rootContext()->setContextProperty("directoryModel",
                                    QVariant::fromValue(object_list));
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
  if (e->key() == Qt::Key_P) {
    emit hidePlayer();
    stop();
  }
}

void Window::clearCurrentDirectoryList() {
  QObjectList object_list =
      rootContext()->contextProperty("directoryModel").value<QObjectList>();
  rootContext()->setContextProperty("directoryModel", nullptr);
  for (QObject* object : object_list) delete object;
}

bool Window::goBack() {
  if (directory_stack_.empty()) {
    clearCurrentDirectoryList();
    return false;
  }
  current_directory_ = directory_stack_.back();
  directory_stack_.pop_back();
  listDirectory();
  return true;
}

void Window::play(ItemModel* item) {
  stop();
  item_data_request_ = cloud_provider_->getItemDataAsync(
      item->item(), [this](IItem::Pointer item) {
        emit runPlayerFromUrl(item->url().c_str());
      });
}

void Window::stop() {
  media_player_.stop();
  download_request_ = nullptr;
}

void Window::uploadFile(QString path) {
  std::cerr << "[DIAG] Uploading file " << path.toStdString() << "\n";
  QUrl url = path;
  upload_request_ = cloud_provider_->uploadFileAsync(
      current_directory_, url.fileName().toStdString(),
      make_unique<UploadFileCallback>(this, url));
}

void Window::downloadFile(ItemModel* i, QUrl path) {
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
