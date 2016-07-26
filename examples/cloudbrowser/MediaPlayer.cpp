/*****************************************************************************
 * MediaPlayer.cpp : MediaPlayer implementation
 *
 *****************************************************************************
 * Copyright (C) 2016-2016 VideoLAN
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

#include "MediaPlayer.h"

#ifdef WITH_VLC

VLCMediaPlayer::VLCMediaPlayer()
    : vlc_instance_(0, nullptr), media_player_(vlc_instance_) {
#ifdef Q_OS_LINUX
  media_player_.setXwindow(winId());
#elif defined Q_OS_WIN
  media_player_.setHwnd(reinterpret_cast<void*>(winId()));
#elif defined Q_OS_DARWIN
  media_player_.setNsobject(reinterpret_cast<void*>(winId()));
#endif

  media_player_.eventManager().onPlaying([this]() { emit started(); });
  media_player_.eventManager().onStopped([this]() { emit stopped(); });
  media_player_.eventManager().onEndReached([this]() { emit endReached(); });
}

void VLCMediaPlayer::pause() { media_player_.pause(); }

void VLCMediaPlayer::play() { media_player_.play(); }

void VLCMediaPlayer::stop() { media_player_.stop(); }

void VLCMediaPlayer::setMedia(const std::string& url) {
  VLC::Media media(vlc_instance_, url, VLC::Media::FromLocation);
  media_player_.setMedia(media);
}

std::unique_ptr<MediaPlayer> MediaPlayer::instance(QWidget* parent) {
  std::unique_ptr<MediaPlayer> player(new VLCMediaPlayer);
  player->setParent(parent);
  return player;
}

#else

QtMediaPlayer::QtMediaPlayer() {
  media_player_.setVideoOutput(this);
  connect(&media_player_, &QMediaPlayer::mediaStatusChanged, this,
          [this](QMediaPlayer::MediaStatus status) {
            if (status == QMediaPlayer::EndOfMedia) emit endReached();
          },
          Qt::QueuedConnection);
}

void QtMediaPlayer::pause() { media_player_.pause(); }

void QtMediaPlayer::play() {
  media_player_.play();
  emit started();
}

void QtMediaPlayer::stop() {
  media_player_.stop();
  emit stopped();
}

void QtMediaPlayer::setMedia(const std::string& url) {
  media_player_.setMedia(QUrl(url.c_str()));
}

std::unique_ptr<MediaPlayer> MediaPlayer::instance(QWidget* parent) {
  std::unique_ptr<MediaPlayer> player(new QtMediaPlayer);
  player->setParent(parent);
  return player;
}

#endif  // WITH_VLC
