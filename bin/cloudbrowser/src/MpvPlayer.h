/*****************************************************************************
 * MpvPlayer.h
 *
 *****************************************************************************
 * Copyright (C) 2018 VideoLAN
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
#ifndef MPVPLAYER_H
#define MPVPLAYER_H

#ifdef WITH_MPV

#include <QQuickFramebufferObject>

struct mpv_handle;
struct mpv_render_context;

class MpvPlayer : public QQuickFramebufferObject {
 public:
  Q_PROPERTY(QString uri READ uri WRITE setUri NOTIFY uriChanged)
  Q_PROPERTY(
      qreal position READ position WRITE setPosition NOTIFY positionChanged)
  Q_PROPERTY(int duration READ duration NOTIFY durationChanged)
  Q_PROPERTY(bool ended READ ended NOTIFY endedChanged)
  Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
  Q_PROPERTY(bool paused READ paused NOTIFY pausedChanged)
  Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY volumeChanged)

  MpvPlayer(QQuickItem *parent = nullptr);
  ~MpvPlayer() override;

  QString uri() const;
  void setUri(QString uri);

  qreal position() const;
  void setPosition(qreal position);

  int volume() const;
  void setVolume(int);

  int duration() const;
  bool ended() const;
  bool playing() const;
  bool paused() const;

  Q_INVOKABLE void play();
  Q_INVOKABLE void pause();

 signals:
  void onUpdate();
  void onInitialized();
  void onEventOccurred();
  void uriChanged();
  void positionChanged();
  void durationChanged();
  void endedChanged();
  void playingChanged();
  void pausedChanged();
  void volumeChanged();

 protected:
  Renderer *createRenderer() const override;

 private:
  Q_OBJECT

  friend class MpvRenderer;

  void eventOccurred();
  void executeLoadFile();

  mpv_handle *mpv_;
  mpv_render_context *mpv_gl_;
  QString uri_;
  qreal position_ = 0;
  bool initialized_ = false;
  bool invoke_load_ = false;
  bool ended_ = false;
  bool playing_ = false;
  bool paused_ = false;
  int volume_ = 100;
  int64_t duration_ = 0;
};

#endif  // WITH_MPV

#endif  // MPVPLAYER_H
