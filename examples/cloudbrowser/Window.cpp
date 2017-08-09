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
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaEnum>
#include <QMetaObject>
#include <QQmlContext>
#include <QQuickItem>
#include <QSettings>
#include <iostream>

#include "MockProvider.h"

#include "Utility/Utility.h"

#ifdef WITH_THUMBNAILER
#include "GenerateThumbnail.h"
#endif

using namespace cloudstorage;

namespace {

std::string read_file(const std::string& path) {
  QFile file(path.c_str());
  file.open(QFile::ReadOnly);
  return file.readAll().constData();
}

}  // namespace

Window::Window(MediaPlayer* media_player)
    : cloud_storage_(ICloudStorage::create()),
      last_played_(-1),
      media_player_(media_player) {
  qRegisterMetaType<ItemPointer>();

  QStringList clouds;
  for (auto p : ICloudStorage::create()->providers())
    clouds.append(p->name().c_str());
  clouds.append("mock");
#ifdef WITH_QTWEBENGINE
  rootContext()->setContextProperty("qtwebengine", QVariant(true));
#else
  rootContext()->setContextProperty("qtwebengine", QVariant(false));
#endif
  rootContext()->setContextProperty("cloudModel", clouds);
  rootContext()->setContextProperty("window", this);
  rootContext()->setContextProperty("directoryModel", &directory_model_);

  setSource(QUrl("qrc:/qml/main.qml"));
  setResizeMode(SizeRootObjectToView);

  if (media_player_) {
    connect(media_player_, &MediaPlayer::started, this, &Window::showPlayer);
    connect(media_player_, &MediaPlayer::stopped, this, &Window::hidePlayer);
    connect(media_player_, &MediaPlayer::endReached, this, &Window::playNext);
  }

  connect(this, &Window::successfullyAuthorized, this,
          &Window::onSuccessfullyAuthorized);
  connect(this, &Window::addedItem, this, &Window::onAddedItem);
  connect(this, &Window::runPlayerFromUrl, this, &Window::onPlayFileFromUrl);
  connect(this, &Window::cloudChanged, this, &Window::listDirectory);
  connect(this, &Window::showPlayer, this,
          [this]() {
            ItemModel* item = directory_model_.get(last_played_);
            if (item && item->item()->type() != IItem::FileType::Audio) {
              if (media_player_)
                emit showWidgetPlayer();
              else
                emit showQmlPlayer();
              contentItem()->setFocus(false);
            }
          },
          Qt::QueuedConnection);
  connect(this, &Window::hidePlayer, this,
          [this]() {
            if (media_player_)
              emit hideWidgetPlayer();
            else
              emit hideQmlPlayer();
            contentItem()->setFocus(true);
          },
          Qt::QueuedConnection);
  connect(this, &Window::runListDirectory, this, [this]() { listDirectory(); },
          Qt::QueuedConnection);
  connect(this, &Window::runClearDirectory, this,
          [this]() { clearCurrentDirectoryList(); }, Qt::QueuedConnection);
  connect(this, &Window::playNext, this,
          [this]() {
            if (last_played_ == -1) return;
            for (int i = last_played_ + 1; i < directory_model_.rowCount();
                 i++) {
              auto type = directory_model_.get(i)->item()->type();
              if (type == IItem::FileType::Audio ||
                  type == IItem::FileType::Video ||
                  type == IItem::FileType::Image) {
                emit currentItemChanged(i);
                play(i);
                break;
              }
            }
          },
          Qt::QueuedConnection);
  connect(
      this, &Window::consentRequired, this,
      [this](QString name) { unauthorized_clouds_.insert(name.toStdString()); },
      Qt::QueuedConnection);
}

Window::~Window() {
  cancelRequests();
  if (clear_directory_.valid()) clear_directory_.get();
  clearCurrentDirectoryList();
  saveCloudAccessToken();
}

void Window::initializeCloud(QString name) {
  if (cloud_provider_ && cloud_provider_->name() == name.toStdString()) return;
  saveCloudAccessToken();
  cancelRequests();
  if (name != "mock")
    cloud_provider_ = cloud_storage_->provider(name.toStdString());
  else
    cloud_provider_ = util::make_unique<cloudstorage::MockProvider>();
  current_directory_ = cloud_provider_->rootDirectory();
  if (initialized_clouds_.find(name.toStdString()) ==
      std::end(initialized_clouds_)) {
    QSettings settings;
    {
      auto lock = stream_lock();
      std::cerr << "[DIAG] Trying to authorize with "
                << settings.value(name).toString().toStdString() << std::endl;
    }
    QString json = settings.value(name + "_hints").toString();
    QJsonObject data = QJsonDocument::fromJson(json.toLocal8Bit()).object();
    auto hints = fromJson(data);
    hints["temporary_directory"] =
        QDir::toNativeSeparators(QDir::tempPath() + "/").toStdString();
    if (name == "amazons3" || name == "mega" || name == "owncloud") {
      hints["login_page"] =
          read_file(":/resources/" + name.toStdString() + "_login.html");
      hints["success_page"] =
          read_file(":/resources/" + name.toStdString() + "_success.html");
    } else {
      hints["success_page"] = read_file(":/resources/default_success.html");
    }
    hints["error_page"] = read_file(":/resources/default_error.html");
    cloud_provider_->initialize({settings.value(name).toString().toStdString(),
                                 util::make_unique<CloudProviderCallback>(this),
                                 nullptr, nullptr, nullptr, hints});
    initialized_clouds_.insert(name.toStdString());
  } else if (unauthorized_clouds_.find(name.toStdString()) !=
             std::end(unauthorized_clouds_)) {
    emit openBrowser(cloud_provider_->authorizeLibraryUrl().c_str());
  }
  emit runListDirectory();
}

void Window::listDirectory() {
  if (!cloud_provider_ || !current_directory_) return;
  clearCurrentDirectoryList();
  list_directory_request_ = cloud_provider_->listDirectoryAsync(
      current_directory_, util::make_unique<::ListDirectoryCallback>(this));
}

void Window::changeCurrentDirectory(int directory_id) {
  auto directory = directory_model_.get(directory_id);
  directory_stack_.push_back(current_directory_);
  current_directory_ = directory->item();

  list_directory_request_ = nullptr;
  startDirectoryClear([this]() { emit runListDirectory(); });
}

void Window::onSuccessfullyAuthorized(QString name) {
  {
    auto lock = stream_lock();
    std::cerr << "[OK] Successfully authorized " << name.toStdString() << "\n";
  }
  auto it = unauthorized_clouds_.find(name.toStdString());
  if (it != std::end(unauthorized_clouds_)) unauthorized_clouds_.erase(it);
}

void Window::onAddedItem(ItemPointer i) { directory_model_.addItem(i, this); }

void Window::onPlayFileFromUrl(QString url) {
  {
    auto lock = stream_lock();
    std::cerr << "[DIAG] Playing url " << url.toStdString() << "\n";
  }
  if (media_player_) {
    media_player_->setMedia(url.toStdString());
    media_player_->play();
  } else {
    setCurrentMedia(url);
    emit playQmlPlayer();
    emit showPlayer();
  }
  {
    auto lock = stream_lock();
    std::cerr << "[DIAG] Set media " << url.toStdString() << "\n";
  }
}

QString Window::movedItem() const {
  if (moved_file_)
    return moved_file_->filename().c_str();
  else
    return "";
}

void Window::setCurrentMedia(QString m) {
  if (current_media_ != m) {
    current_media_ = m;
    emit currentMediaChanged();
  }
}

std::string Window::escapeFileName(std::string name) {
  std::vector<char> problematic_chars = {'\\', '/', '<',  '>', '|', '"', ':',
                                         '?',  '*', '\'', '(', ')', '#'};
  for (char& c : name)
    if (std::find(problematic_chars.begin(), problematic_chars.end(), c) !=
        std::end(problematic_chars))
      c = 'x';
  return name;
}

void Window::keyPressEvent(QKeyEvent* e) {
  QQuickView::keyPressEvent(e);
  if (e->isAccepted() && contentItem()->hasFocus()) return;
  if (e->key() == Qt::Key_Q || e->key() == Qt::Key_Back) {
    stop();
    e->accept();
  } else if (e->key() == Qt::Key_P) {
    if (media_player_)
      media_player_->pause();
    else
      emit pauseQmlPlayer();
    e->accept();
  }
}

void Window::clearCurrentDirectoryList() {
  directory_model_.clear();
  last_played_ = -1;
}

void Window::saveCloudAccessToken() {
  if (cloud_provider_) {
    clearCurrentDirectoryList();
    QSettings settings;
    std::string data = QJsonDocument(toJson(cloud_provider_->hints()))
                           .toJson(QJsonDocument::Compact)
                           .toStdString();
    settings.setValue((cloud_provider_->name() + "_hints").c_str(),
                      data.c_str());
  }
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

void Window::cancelRequests() {
  if (list_directory_request_) list_directory_request_->cancel();
  if (download_request_) download_request_->cancel();
  if (upload_request_) upload_request_->cancel();
  if (item_data_request_) item_data_request_->cancel();
  if (delete_item_request_) delete_item_request_->cancel();
  if (create_directory_request_) create_directory_request_->cancel();
  if (move_item_request_) move_item_request_->cancel();
  if (rename_item_request_) rename_item_request_->cancel();
  moved_file_ = nullptr;
  emit movedItemChanged();
}

ICloudProvider::Hints Window::fromJson(const QJsonObject& json) const {
  ICloudProvider::Hints hints;
  for (QString key : json.keys())
    hints[key.toStdString()] = json[key].toString().toStdString();
  return hints;
}

QJsonObject Window::toJson(const ICloudProvider::Hints& map) const {
  QJsonObject result;
  for (auto it = map.begin(); it != map.end(); it++)
    result[it->first.c_str()] = it->second.c_str();
  return result;
}

bool Window::goBack() {
  if (list_directory_request_) list_directory_request_->cancel();
  if (directory_stack_.empty()) {
    moved_file_ = nullptr;
    cloud_provider_ = nullptr;
    emit movedItemChanged();
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
  if (item->item()->type() != IItem::FileType::Audio)
    contentItem()->setFocus(false);
  last_played_ = item_id;
  item_data_request_ = cloud_provider_->getItemDataAsync(
      item->item()->id(), [this](EitherError<IItem> i) {
        if (i.right()) emit runPlayerFromUrl(i.right()->url().c_str());
      });
}

void Window::stop() {
  {
    auto lock = stream_lock();
    std::cerr << "[DIAG] Trying to stop player\n";
  }
  if (media_player_)
    media_player_->stop();
  else {
    emit stopQmlPlayer();
    emit hidePlayer();
  }
  {
    auto lock = stream_lock();
    std::cerr << "[DIAG] Stopped\n";
  }
}

void Window::uploadFile(QString path) {
  {
    auto lock = stream_lock();
    std::cerr << "[DIAG] Uploading file " << path.toStdString() << "\n";
  }
  QUrl url = path;
  upload_request_ = cloud_provider_->uploadFileAsync(
      current_directory_, url.fileName().toStdString(),
      util::make_unique<::UploadFileCallback>(this, url));
}

void Window::downloadFile(int item_id, QUrl path) {
  ItemModel* i = directory_model_.get(item_id);
  download_request_ = cloud_provider_->downloadFileAsync(
      i->item(), util::make_unique<::DownloadFileCallback>(
                     this,
                     path.toLocalFile().toStdString() + "/" +
                         escapeFileName(i->item()->filename())));

  auto lock = stream_lock();
  std::cerr << "[DIAG] Downloading file " << path.toLocalFile().toStdString()
            << "\n";
}

void Window::deleteItem(int item_id) {
  ItemModel* i = directory_model_.get(item_id);
  delete_item_request_ =
      cloud_provider_->deleteItemAsync(i->item(), [this](EitherError<void> e) {
        auto lock = stream_lock();
        if (!e.left())
          std::cerr << "[DIAG] Successfully deleted file\n";
        else
          std::cerr << "[FAIL] Failed to delete file " << e.left()->code_
                    << ": " << e.left()->description_ << "\n";
      });
}

void Window::createDirectory(QString name) {
  create_directory_request_ = cloud_provider_->createDirectoryAsync(
      current_directory_, name.toStdString(), [this](EitherError<IItem> item) {
        auto lock = stream_lock();
        if (item.right())
          std::cerr << "[DIAG] Successfully created directory\n";
        else
          std::cerr << "[FAIL] Failed to create directory "
                    << item.left()->code_ << ": " << item.left()->description_
                    << "\n";
      });
}

void Window::markMovedItem(int item_id) {
  if (moved_file_) {
    move_item_request_ = cloud_provider_->moveItemAsync(
        moved_file_, current_directory_, [this](EitherError<void> e) {
          auto lock = stream_lock();
          if (!e.left())
            std::cerr << "[DIAG] Successfully moved file\n";
          else
            std::cerr << "[FAIL] Failed to move file " << e.left()->code_
                      << ": " << e.left()->description_ << "\n";
        });
    moved_file_ = nullptr;
  } else if (item_id != -1) {
    moved_file_ = directory_model_.get(item_id)->item();
  }
  emit movedItemChanged();
}

void Window::renameItem(int item_id, QString name) {
  auto item = directory_model_.get(item_id)->item();
  rename_item_request_ = cloud_provider_->renameItemAsync(
      item, name.toStdString(), [this](EitherError<void> e) {
        auto lock = stream_lock();
        if (!e.left())
          std::cerr << "[DIAG] Successfully renamed file\n";
        else
          std::cerr << "[FAIL] Failed to rename file " << e.left()->code_
                    << ": " << e.left()->description_ << "\n";
      });
}

ItemModel::ItemModel(IItem::Pointer item, ICloudProvider::Pointer p, Window* w)
    : item_(item),
      provider_(p),
      thread_info_(std::make_shared<ThreadInfo>()),
      window_(w) {
  connect(this, &ItemModel::receivedImage, this,
          [this]() {
            fetchThumbnail();
            emit thumbnailChanged();
          },
          Qt::QueuedConnection);
#ifdef WITH_THUMBNAILER
  connect(this, &ItemModel::failedImage, this,
          [=]() {
            auto tinfo = thread_info_;
            thumbnail_thread_ = std::thread([=] {
              auto thumbnail = cloudstorage::generate_thumbnail(item_);
              std::lock_guard<std::mutex> lock(tinfo->lock_);
              if (!tinfo->nuked_ && thumbnail.right()) {
                auto data = *thumbnail.right();
                QFile file(QDir::tempPath() + "/" +
                           Window::escapeFileName(item_->filename()).c_str() +
                           ".thumbnail");
                file.open(QFile::WriteOnly);
                file.write(data.data(), data.length());
                emit receivedImage();
              }
            });
          },
          Qt::QueuedConnection);
#endif
}

ItemModel::~ItemModel() {
  if (thumbnail_request_) thumbnail_request_->cancel();
  std::lock_guard<std::mutex> lock(thread_info_->lock_);
  thread_info_->nuked_ = true;
  try {
    thumbnail_thread_.detach();
  } catch (const std::exception&) {
  }
}

void ItemModel::fetchThumbnail() {
  auto cache = QDir::tempPath() + "/" +
               Window::escapeFileName(item_->filename()).c_str() + ".thumbnail";
  if (QFile::exists(cache)) {
    thumbnail_ = QUrl::fromLocalFile(cache).toString();
  } else if (!thumbnail_request_)
    thumbnail_request_ = provider_->getThumbnailAsync(
        item_, util::make_unique<DownloadThumbnailCallback>(this));
}

int DirectoryModel::rowCount(const QModelIndex&) const { return list_.size(); }

QVariant DirectoryModel::data(const QModelIndex& id, int) const {
  if (static_cast<uint32_t>(id.row()) >= list_.size()) return QVariant();
  auto model = list_[id.row()].get();
  model->fetchThumbnail();

  QVariantMap dict;
  dict["thumbnail"] = model->thumbnail();
  dict["name"] = model->item()->filename().c_str();
  dict["is_directory"] = model->item()->type() == IItem::FileType::Directory;
  dict["icon"] = typeToIcon(model->item()->type());
  return dict;
}

void DirectoryModel::addItem(IItem::Pointer item, Window* w) {
  beginInsertRows(QModelIndex(), rowCount(), rowCount());
  auto model = util::make_unique<ItemModel>(item, w->cloud_provider_, w);
  list_.push_back(std::move(model));
  endInsertRows();
  int idx = rowCount() - 1;
  connect(list_.back().get(), &ItemModel::thumbnailChanged, this,
          [this, idx]() { emit dataChanged(index(idx, 0), index(idx, 0)); },
          Qt::QueuedConnection);
}

ItemModel* DirectoryModel::get(int id) const {
  if (id < 0 || id >= (int)list_.size()) return nullptr;
  return list_[id].get();
}

void DirectoryModel::clear() {
  beginRemoveRows(QModelIndex(), 0, rowCount() - 1);
  list_.clear();
  endRemoveRows();
}

QString DirectoryModel::typeToIcon(IItem::FileType type) {
  switch (type) {
    case IItem::FileType::Audio:
      return "qrc:/resources/audio.png";
    case IItem::FileType::Directory:
      return "qrc:/resources/directory.png";
    case IItem::FileType::Image:
      return "qrc:/resources/image.png";
    case IItem::FileType::Video:
      return "qrc:/resources/video.png";
    case IItem::FileType::Unknown:
      return "qrc:/resources/file.png";
    default:
      return "";
  }
}
