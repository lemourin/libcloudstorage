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
#include <QQmlContext>
#include <QSettings>

using namespace cloudstorage;

namespace {

class ListDirectoryCallback : public ListDirectoryRequest::ICallback {
 public:
  ListDirectoryCallback(Window* w) : window_(w) {}

  void receivedItem(IItem::Pointer item) { emit window_->addedItem(item); }

  void done(const std::vector<IItem::Pointer>&) {}

  void error(const std::string& str) { qDebug() << str.c_str(); }

 private:
  Window* window_;
};

template <typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

}  // namespace

Window::Window() {
  qRegisterMetaType<IItem::Pointer>();

  QStringList clouds;
  for (auto p : CloudStorage().providers()) clouds.append(p->name().c_str());
  rootContext()->setContextProperty("cloudModel", clouds);
  rootContext()->setContextProperty("window", this);
  rootContext()->setContextProperty(
      "directoryModel", QVariant::fromValue(current_directory_list_));

  setSource(QUrl("qrc:/main.qml"));
  setResizeMode(SizeRootObjectToView);

  connect(this, &Window::successfullyAuthorized, this,
          &Window::onSuccessfullyAuthorized);
  connect(this, &Window::addedItem, this, &Window::onAddedItem);
}

Window::~Window() { clearCurrentDirectoryList(); }

void Window::initializeCloud(QString name) {
  QSettings settings;
  cloud_provider_ = CloudStorage().provider(name.toStdString());
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
  qDebug() << "[OK] Successfully authorized" << cloud_provider_->name().c_str();
  current_directory_ = cloud_provider_->rootDirectory();
  listDirectory();
}

void Window::onAddedItem(IItem::Pointer i) {
  ItemModel* model = new ItemModel(i);
  model->setParent(this);
  current_directory_list_.append(model);
  rootContext()->setContextProperty(
      "directoryModel", QVariant::fromValue(current_directory_list_));
}

void Window::onRunPlayer() {
  if (media_player_.state() != QMediaPlayer::PlayingState) {
    if (media_player_.mediaStream() == nullptr)
      media_player_.setMedia(nullptr, device_.get());
    media_player_.play();
  }
}

void Window::onPausePlayer() {
  if (media_player_.state() == QMediaPlayer::PlayingState) {
    media_player_.pause();
  }
}

void Window::clearCurrentDirectoryList() {
  rootContext()->setContextProperty("directoryModel", nullptr);
  for (QObject* obj : current_directory_list_) delete obj;
  current_directory_list_.clear();
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

  device_ = make_unique<InputDevice>();
  download_request_ = cloud_provider_->downloadFileAsync(
      item->item(), make_unique<DownloadFileCallback>(device_.get()));
  connect(device_.get(), &InputDevice::runPlayer, this, &Window::onRunPlayer);
  connect(device_.get(), &InputDevice::pausePlayer, this,
          &Window::onPausePlayer);
}

void Window::stop() {
  media_player_.stop();
  media_player_.setMedia(nullptr, nullptr);
  download_request_ = nullptr;
}

bool Window::playing() {
  return media_player_.state() != QMediaPlayer::StoppedState;
}

CloudProviderCallback::CloudProviderCallback(Window* w) : window_(w) {}

ICloudProvider::ICallback::Status CloudProviderCallback::userConsentRequired(
    const ICloudProvider& p) {
  emit window_->openBrowser(p.authorizeLibraryUrl().c_str());
  return Status::WaitForAuthorizationCode;
}

void CloudProviderCallback::accepted(const ICloudProvider& drive) {
  QSettings settings;
  settings.setValue(drive.name().c_str(), drive.token().c_str());
  emit window_->closeBrowser();
  emit window_->successfullyAuthorized();
}

void CloudProviderCallback::declined(const ICloudProvider&) {
  emit window_->closeBrowser();
}

void CloudProviderCallback::error(const ICloudProvider&,
                                  const std::string& desc) {
  qDebug() << "[FAIL]" << desc.c_str();
}

ItemModel::ItemModel(IItem::Pointer item) : item_(item) {}

ItemModel::~ItemModel() {}
