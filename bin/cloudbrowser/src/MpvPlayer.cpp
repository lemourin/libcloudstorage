/*****************************************************************************
 * MpvPlayer.cpp
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
#include "MpvPlayer.h"

#ifdef WITH_MPV

#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QQuickWindow>
#include <QSGRenderNode>

#include <mpv/client.h>
#include <mpv/render_gl.h>

#include "Utility/Utility.h"

const int DURATION_ID = 1;
const int POSITION_ID = 2;
const int IDLE_ID = 3;
const int TRACK_LIST_ID = 4;
const int CACHE_TIME_ID = 5;

namespace {
void on_mpv_events(void *d) {
  emit reinterpret_cast<MpvPlayer *>(d)->onEventOccurred();
}

static void *get_proc_address_mpv(void *, const char *name) {
  QOpenGLContext *glctx = QOpenGLContext::currentContext();
  if (!glctx) return nullptr;

  return reinterpret_cast<void *>(glctx->getProcAddress(QByteArray(name)));
}

}  // namespace

class MpvRenderer : public QQuickFramebufferObject::Renderer {
 public:
  MpvRenderer(const std::shared_ptr<mpv_handle> &mpv) : mpv_(mpv) {
    mpv_opengl_init_params gl_init_params{get_proc_address_mpv, nullptr,
                                          nullptr};
    mpv_render_param params[]{
        {MPV_RENDER_PARAM_API_TYPE,
         const_cast<char *>(MPV_RENDER_API_TYPE_OPENGL)},
        {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_init_params},
        {MPV_RENDER_PARAM_INVALID, nullptr}};
    if (mpv_render_context_create(&mpv_gl_, mpv_.get(), params) < 0)
      throw std::runtime_error("failed to initialize mpv GL context");
  }

  ~MpvRenderer() override { mpv_render_context_free(mpv_gl_); }

  void render() override {
    mpv_opengl_fbo mpfbo;
    mpfbo.fbo = framebufferObject()->handle();
    mpfbo.w = framebufferObject()->width();
    mpfbo.h = framebufferObject()->height();
    mpfbo.internal_format = 0;

    mpv_render_param params[] = {{MPV_RENDER_PARAM_OPENGL_FBO, &mpfbo},
                                 {MPV_RENDER_PARAM_INVALID, nullptr}};
    mpv_render_context_render(mpv_gl_, params);
    update();
  }

  QOpenGLFramebufferObject *createFramebufferObject(
      const QSize &size) override {
    return new QOpenGLFramebufferObject(size);
  }

 private:
  QRectF rect_;
  std::shared_ptr<mpv_handle> mpv_;
  mpv_render_context *mpv_gl_;
};

MpvPlayer::MpvPlayer(QQuickItem *parent)
    : QQuickFramebufferObject(parent),
      mpv_(std::shared_ptr<mpv_handle>(mpv_create(), mpv_terminate_destroy)) {
  if (!mpv_) throw std::runtime_error("could not create mpv context");

  mpv_set_option_string(mpv_.get(), "video-timing-offset", "0");
  mpv_observe_property(mpv_.get(), POSITION_ID, "time-pos", MPV_FORMAT_INT64);
  mpv_observe_property(mpv_.get(), IDLE_ID, "core-idle", MPV_FORMAT_FLAG);
  mpv_observe_property(mpv_.get(), CACHE_TIME_ID, "demuxer-cache-time",
                       MPV_FORMAT_NODE);
  mpv_request_log_messages(mpv_.get(), "warn");

  if (mpv_initialize(mpv_.get()) < 0)
    throw std::runtime_error("could not initialize mpv context");

  mpv_set_option_string(mpv_.get(), "hwdec", "auto");

  connect(this, &MpvPlayer::onInitialized, this,
          [=] {
            initialized_ = true;
            if (invoke_load_) executeLoadFile();
          },
          Qt::QueuedConnection);
  connect(this, &MpvPlayer::onEventOccurred, this, &MpvPlayer::eventOccurred,
          Qt::QueuedConnection);

  mpv_set_wakeup_callback(mpv_.get(), on_mpv_events, this);
}

QString MpvPlayer::uri() const { return uri_; }

void MpvPlayer::setUri(QString uri) {
  uri_ = uri;
  emit uriChanged();
  if (!initialized_) {
    invoke_load_ = true;
  } else {
    executeLoadFile();
  }
}

qreal MpvPlayer::position() const { return position_; }

void MpvPlayer::setPosition(qreal position) {
  position_ = position;
  emit positionChanged();
  int64_t p = position * duration() / 1000;
  mpv_set_property_async(mpv_.get(), POSITION_ID, "playback-time",
                         MPV_FORMAT_INT64, &p);
}

int MpvPlayer::volume() const { return volume_; }

void MpvPlayer::setVolume(int v) {
  if (volume_ != v) {
    volume_ = v;
    emit volumeChanged();
    int64_t volume = v;
    mpv_set_property_async(mpv_.get(), 0, "volume", MPV_FORMAT_INT64, &volume);
  }
}

int MpvPlayer::duration() const { return duration_; }

bool MpvPlayer::ended() const { return ended_; }

bool MpvPlayer::playing() const { return playing_; }

bool MpvPlayer::paused() const { return paused_; }

void MpvPlayer::play() {
  int flag = false;
  mpv_set_property_async(mpv_.get(), 0, "pause", MPV_FORMAT_FLAG, &flag);
  paused_ = false;
  emit pausedChanged();
}

void MpvPlayer::pause() {
  int flag = true;
  mpv_set_property_async(mpv_.get(), 0, "pause", MPV_FORMAT_FLAG, &flag);
  paused_ = true;
  emit pausedChanged();
}

void MpvPlayer::set_subtitle_track(int track) {
  if (track > 0) {
    mpv_set_property_async(mpv_.get(), 0, "sid", MPV_FORMAT_INT64,
                           &subtitle_tracks_id_[track]);
  } else {
    const char *value = "no";
    mpv_set_property_async(mpv_.get(), 0, "sid", MPV_FORMAT_STRING, &value);
  }
}

void MpvPlayer::set_audio_track(int track) {
  mpv_set_property_async(mpv_.get(), 0, "aid", MPV_FORMAT_INT64,
                         &audio_tracks_id_[track]);
}

QQuickFramebufferObject::Renderer *MpvPlayer::createRenderer() const {
  emit const_cast<MpvPlayer *>(this)->onInitialized();
  return new MpvRenderer(mpv_);
}

void MpvPlayer::eventOccurred() {
  while (auto event = mpv_wait_event(mpv_.get(), 0)) {
    if (event->event_id == MPV_EVENT_GET_PROPERTY_REPLY) {
      auto property = reinterpret_cast<mpv_event_property *>(event->data);
      if (event->reply_userdata == DURATION_ID) {
        duration_ = 1000 * *reinterpret_cast<int64_t *>(property->data);
        emit durationChanged();
      } else if (event->reply_userdata == TRACK_LIST_ID) {
        auto track_list = reinterpret_cast<mpv_node *>(property->data)->u.list;
        subtitle_tracks_ = QStringList{"Disabled"};
        audio_tracks_ = QStringList{};
        subtitle_tracks_id_ = {-1};
        audio_tracks_id_ = {};
        for (int i = 0; i < track_list->num; i++) {
          auto track = track_list->values[i].u.list;
          std::string type, title, lang;
          int64_t id;
          for (int j = 0; j < track->num; j++) {
            if (track->keys[j] == std::string("type"))
              type = track->values[j].u.string;
            else if (track->keys[j] == std::string("title"))
              title = track->values[j].u.string;
            else if (track->keys[j] == std::string("lang"))
              lang = track->values[j].u.string;
            else if (track->keys[j] == std::string("id"))
              id = track->values[j].u.int64;
          }
          if (type == "sub") {
            subtitle_tracks_.push_back(
                ((title.empty()
                      ? "Track " + std::to_string(subtitle_tracks_.size())
                      : title) +
                 (lang.empty() ? "" : " [" + lang + "]"))
                    .c_str());
            subtitle_tracks_id_.push_back(id);
          } else if (type == "audio") {
            audio_tracks_.push_back(
                ((title.empty()
                      ? "Track " + std::to_string(audio_tracks_.size() + 1)
                      : title) +
                 (lang.empty() ? "" : " [" + lang + "]"))
                    .c_str());
            audio_tracks_id_.push_back(id);
          }
        }
        emit subtitleTracksChanged();
        emit audioTracksChanged();
      }
    } else if (event->event_id == MPV_EVENT_FILE_LOADED) {
      mpv_get_property_async(mpv_.get(), DURATION_ID, "duration",
                             MPV_FORMAT_INT64);
      mpv_get_property_async(mpv_.get(), TRACK_LIST_ID, "track-list",
                             MPV_FORMAT_NODE);
    } else if (event->event_id == MPV_EVENT_PROPERTY_CHANGE) {
      auto property = reinterpret_cast<mpv_event_property *>(event->data);
      if (event->reply_userdata == POSITION_ID && property->data) {
        position_ =
            1000.0 * *reinterpret_cast<int64_t *>(property->data) / duration();
        emit positionChanged();
      } else if (event->reply_userdata == IDLE_ID) {
        playing_ = !*reinterpret_cast<int *>(property->data);
        emit playingChanged();
      } else if (event->reply_userdata == CACHE_TIME_ID && property->data) {
        cache_position_ =
            1000 * reinterpret_cast<mpv_node *>(property->data)->u.double_ /
            duration();
        emit cachePositionChanged();
      }
    } else if (event->event_id == MPV_EVENT_END_FILE) {
      auto e = reinterpret_cast<mpv_event_end_file *>(event->data);
      if (e->reason == MPV_END_FILE_REASON_EOF ||
          e->reason == MPV_END_FILE_REASON_ERROR) {
        ended_ = true;
        emit endedChanged();
      }
    } else if (event->event_id == MPV_EVENT_LOG_MESSAGE) {
      auto e = reinterpret_cast<mpv_event_log_message *>(event->data);
      std::string text = e->text;
      if (text.length() > 0)
        cloudstorage::util::log(text.substr(0, text.length() - 1));
    } else if (event->event_id == MPV_EVENT_NONE) {
      break;
    }
  }
}

void MpvPlayer::executeLoadFile() {
  mpv_command_string(mpv_.get(),
                     ("loadfile \"" + uri_.toStdString() + "\"").c_str());
  ended_ = false;
}

#endif  // WITH_MPV
