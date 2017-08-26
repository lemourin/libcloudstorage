/*****************************************************************************
 * Item.cpp : implementation of Item
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

#include "Item.h"

#include "Utility/Utility.h"

#include <algorithm>

#define SIZE(x) (sizeof(x) / sizeof(x[0]))

const std::string VIDEO_EXTENSIONS[] = {
    "3g2", "3gp", "asf", "avi", "flv", "m4v", "mkv", "mov",
    "mp4", "mpg", "rm",  "srt", "swf", "vob", "wmv", "webm"};
const std::string AUDIO_EXTENSIONS[] = {"aif", "iff", "m3u", "m4a", "mid",
                                        "mp3", "mpa", "wav", "wma"};
const std::string IMAGE_EXTENSIONS[] = {
    "bmp", "dds", "gif",  "jpg", "png", "psd", "pspimage", "tga",
    "thm", "tif", "tiff", "yuv", "ai",  "eps", "ps",       "svg"};

namespace cloudstorage {

namespace {

bool find(const std::string* array, int length, std::string str) {
  for (char& c : str) c = std::tolower(c);
  for (int i = 0; i < length; i++)
    if (array[i] == str) return true;
  return false;
}

}  // namespace

Item::Item(std::string filename, std::string id, size_t size, FileType type)
    : filename_(filename),
      id_(id),
      url_(),
      size_(size),
      thumbnail_url_(),
      type_(type),
      is_hidden_(false) {
  if (type_ == IItem::FileType::Unknown) type_ = fromExtension(extension());
}

std::string Item::filename() const { return filename_; }

std::string Item::extension() const {
  return filename_.substr(filename_.find_last_of('.') + 1, std::string::npos);
}

std::string Item::id() const { return id_; }

std::string Item::url() const { return url_; }

size_t Item::size() const { return size_; }

void Item::set_url(std::string url) { url_ = url; }

std::string Item::thumbnail_url() const { return thumbnail_url_; }

void Item::set_thumbnail_url(std::string url) { thumbnail_url_ = url; }

bool Item::is_hidden() const { return is_hidden_; }

void Item::set_hidden(bool e) { is_hidden_ = e; }

IItem::FileType Item::type() const { return type_; }

void Item::set_type(FileType t) { type_ = t; }

const std::vector<std::string>& Item::parents() const { return parents_; }

void Item::set_parents(const std::vector<std::string>& parents) {
  parents_ = parents;
}

IItem::FileType Item::fromMimeType(const std::string& mime_type) {
  std::string type = mime_type.substr(0, mime_type.find_first_of('/'));
  if (type == "audio")
    return IItem::FileType::Audio;
  else if (type == "video")
    return IItem::FileType::Video;
  else if (type == "image")
    return IItem::FileType::Image;
  else
    return IItem::FileType::Unknown;
}

IItem::FileType Item::fromExtension(const std::string& extension) {
  if (find(VIDEO_EXTENSIONS, SIZE(VIDEO_EXTENSIONS), extension))
    return IItem::FileType::Video;
  else if (find(AUDIO_EXTENSIONS, SIZE(AUDIO_EXTENSIONS), extension))
    return IItem::FileType::Audio;
  else if (find(IMAGE_EXTENSIONS, SIZE(IMAGE_EXTENSIONS), extension))
    return IItem::FileType::Image;
  else
    return IItem::FileType::Unknown;
}

}  // namespace cloudstorage
