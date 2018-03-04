/*****************************************************************************
 * GenerateThumbnail.cpp
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

#ifdef WITH_THUMBNAILER

#include "GenerateThumbnail.h"
#include "IHttp.h"
#include "IRequest.h"
#include "Utility/Utility.h"

#include <sstream>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

namespace cloudstorage {

const int THUMBNAIL_SIZE = 256;
const int MAX_RETRY_COUNT = 10;

namespace {

template <class T>
class UniquePtr {
 public:
  UniquePtr(T* pointer, std::function<void(T**)> deleter)
      : pointer_(pointer), deleter_(deleter) {}
  ~UniquePtr() {
    if (pointer_) deleter_(&pointer_);
  }

  T* operator->() const { return pointer_; }

  T*& get() { return pointer_; }

 private:
  T* pointer_;
  std::function<void(T**)> deleter_;
};

template <class T>
using Pointer = UniquePtr<T>;

template <class T>
Pointer<T> make(T* d, std::function<void(T**)> f) {
  return Pointer<T>(d, f);
}

template <class T>
Pointer<T> make(T* d, std::function<void(T*)> f) {
  return Pointer<T>(d, [=](T** d) { f(*d); });
}

std::string av_error(int err) {
  char buffer[AV_ERROR_MAX_STRING_SIZE + 1] = {};
  if (av_strerror(err, buffer, AV_ERROR_MAX_STRING_SIZE) < 0)
    return "invalid error";
  else
    return buffer;
}

void check(int code, const std::string& call) {
  if (code < 0) throw std::logic_error(call + " (" + av_error(code) + ")");
}

}  // namespace

EitherError<std::string> generate_thumbnail(
    const std::string& url,
    std::function<bool(std::chrono::system_clock::time_point)> interrupt) {
  try {
    std::string effective_url = url;
#ifdef _WIN32
    const char* file = "file:///";
#else
    const char* file = "file://";
#endif
    const auto length = strlen(file);
    if (url.substr(0, length) == file) effective_url = url.substr(length);
    av_register_all();
    check(avformat_network_init(), "avformat_network_init");
    auto context =
        make<AVFormatContext>(avformat_alloc_context(), avformat_free_context);
    struct CallbackData {
      std::function<bool(std::chrono::system_clock::time_point)> interrupt_;
      std::chrono::system_clock::time_point start_time_;
    } data = {interrupt, std::chrono::system_clock::now()};
    context->interrupt_callback.opaque = &data;
    context->interrupt_callback.callback = [](void* t) -> int {
      auto d = reinterpret_cast<CallbackData*>(t);
      return d->interrupt_(d->start_time_);
    };
    auto network_guard = make<std::nullptr_t>(
        nullptr, [](std::nullptr_t*) { avformat_network_deinit(); });
    check(avformat_open_input(&context.get(), effective_url.c_str(), nullptr,
                              nullptr),
          "avformat_open_input");
    auto input_guard =
        make<AVFormatContext*>(&context.get(), avformat_close_input);
    check(avformat_find_stream_info(context.get(), nullptr),
          "avformat_find_stream_info");
    auto idx = av_find_best_stream(context.get(), AVMEDIA_TYPE_VIDEO, -1, -1,
                                   nullptr, 0);
    check(idx, "av_find_best_stream");
    auto codec =
        avcodec_find_decoder(context->streams[idx]->codecpar->codec_id);
    if (!codec) throw std::logic_error("decoder not found");
    auto codec_context = make<AVCodecContext>(avcodec_alloc_context3(codec),
                                              avcodec_free_context);
    check(avcodec_parameters_to_context(codec_context.get(),
                                        context->streams[idx]->codecpar),
          "avcodec_parameters_to_context");
    check(avcodec_open2(codec_context.get(), codec, nullptr), "avcodec_open2");
    check(av_seek_frame(context.get(), -1, context->duration / 2, 0),
          "av_seek_frame");
    auto frame = make<AVFrame>(av_frame_alloc(), av_frame_free);
    AVPacket packet;
    av_init_packet(&packet);
    int retry_count = 0;
    bool generated_frame = false;
    while (!interrupt(data.start_time_) && retry_count < MAX_RETRY_COUNT &&
           !generated_frame) {
      check(av_read_frame(context.get(), &packet), "av_read_frame");
      auto send_packet = avcodec_send_packet(codec_context.get(), &packet);
      if (send_packet != 0) {
        retry_count++;
        continue;
      }
      auto code = avcodec_receive_frame(codec_context.get(), frame.get());
      if (code == 0)
        generated_frame = true;
      else if (code != -EAGAIN)
        check(code, "avcodec_receive_frame");
    }
    if (!generated_frame) throw std::logic_error("failed to fetch frame");
    int thumbnail_width, thumbnail_height;
    if (codec_context->width > codec_context->height) {
      thumbnail_width = THUMBNAIL_SIZE;
      thumbnail_height =
          codec_context->height * THUMBNAIL_SIZE / codec_context->width;
    } else {
      thumbnail_width =
          codec_context->height * THUMBNAIL_SIZE / codec_context->width;
      thumbnail_height = THUMBNAIL_SIZE;
    }
    auto sws_context = make<SwsContext>(
        sws_getContext(codec_context->width, codec_context->height,
                       codec_context->pix_fmt, thumbnail_width,
                       thumbnail_height, AV_PIX_FMT_RGBA, SWS_BICUBIC, nullptr,
                       nullptr, nullptr),
        sws_freeContext);
    auto rgb_frame = make<AVFrame>(av_frame_alloc(), av_frame_free);
    rgb_frame->format = AV_PIX_FMT_RGBA;
    rgb_frame->width = thumbnail_width;
    rgb_frame->height = thumbnail_height;
    check(av_image_alloc(rgb_frame->data, rgb_frame->linesize, frame->width,
                         frame->height, AV_PIX_FMT_RGBA, 32),
          "av_image_alloc");
    auto image_data_guard = make<std::nullptr_t>(
        nullptr, [&](std::nullptr_t*) { av_freep(&rgb_frame->data); });
    check(sws_scale(sws_context.get(), frame->data, frame->linesize, 0,
                    frame->height, rgb_frame->data, rgb_frame->linesize),
          "sws_scale");
    auto png_codec = avcodec_find_encoder(AV_CODEC_ID_PNG);
    if (!png_codec) throw std::logic_error("png codec not found");
    auto png_context = make<AVCodecContext>(avcodec_alloc_context3(png_codec),
                                            avcodec_free_context);
    png_context->time_base = {1, 24};
    png_context->pix_fmt = AV_PIX_FMT_RGBA;
    png_context->width = thumbnail_width;
    png_context->height = thumbnail_height;
    check(avcodec_open2(png_context.get(), png_codec, nullptr),
          "avcodec_open2");
    check(avcodec_send_frame(png_context.get(), rgb_frame.get()),
          "avcodec_send_frame");
    check(avcodec_receive_packet(png_context.get(), &packet),
          "avcodec_receive_packet");
    std::stringstream output;
    output.write(reinterpret_cast<char*>(packet.data), packet.size);
    return output.str();
  } catch (const std::exception& e) {
    return Error{IHttpRequest::Failure, e.what()};
  }
}

}  // namespace cloudstorage

#endif  // WITH_THUMBNAILER
