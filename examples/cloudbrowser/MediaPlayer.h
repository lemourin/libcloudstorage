/*****************************************************************************
 * MediaPlayer.h : MediaPlayer headers
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

#ifndef MEDIAPLAYER_H
#define MEDIAPLAYER_H

#include <QObject>
#include <memory>

#ifdef WITH_VLC
#include <QWidget>
#include <vlcpp/vlc.hpp>

using Widget = QWidget;
#elif WITH_QTMULTIMEDIAWIDGETS
#include <QMediaContent>
#include <QMediaPlayer>
#include <QVideoWidget>

using Widget = QVideoWidget;
#else
using Widget = QObject;
#endif

class QWidget;

class MediaPlayer : public Widget {
 public:
  virtual void pause() = 0;
  virtual void play() = 0;
  virtual void stop() = 0;
  virtual void setMedia(const std::string&) = 0;

  static std::unique_ptr<MediaPlayer> instance(QWidget*);

 signals:
  void started();
  void stopped();
  void endReached();

 private:
  Q_OBJECT
};

#ifdef WITH_VLC

class VLCMediaPlayer : public MediaPlayer {
 public:
  VLCMediaPlayer();

  void pause();
  void play();
  void stop();
  void setMedia(const std::string&);

 private:
  VLC::Instance vlc_instance_;
  VLC::MediaPlayer media_player_;
};

#elif WITH_QTMULTIMEDIAWIDGETS

class QtMediaPlayer : public MediaPlayer {
 public:
  QtMediaPlayer();

  void pause();
  void play();
  void stop();
  void setMedia(const std::string&);

 private:
  QMediaPlayer media_player_;
};

#endif

#endif  // MEDIAPLAYER_H
