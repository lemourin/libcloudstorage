/*****************************************************************************
 * Utility.cpp
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

#include "Utility.h"

#include <cstdlib>
#include <cstring>
#include <unordered_map>

namespace cloudstorage {

const std::unordered_map<std::string, std::string> MIME_TYPE = {
    {"aac", "audio/aac"},   {"avi", "video/x-msvideo"},
    {"gif", "image/gif"},   {"jpeg", "image/jpeg"},
    {"jpg", "image/jpeg"},  {"mpeg", "video/mpeg"},
    {"oga", "audio/ogg"},   {"ogv", "video/ogg"},
    {"png", "image/png"},   {"svg", "image/svg+xml"},
    {"tif", "image/tiff"},  {"tiff", "image/tiff"},
    {"wav", "audio-x/wav"}, {"weba", "audio/webm"},
    {"webm", "video/webm"}, {"webp", "image/webp"},
    {"3gp", "video/3gpp"},  {"3g2", "video/3gpp2"},
    {"mp4", "video/mp4"},   {"mkv", "video/webm"}};

namespace util {

namespace {

std::string to_lower(std::string str) {
  for (char& c : str) c = tolower(c);
  return str;
}

}  // namespace

std::string remove_whitespace(const std::string& str) {
  std::string result;
  for (char c : str)
    if (!isspace(c)) result += c;
  return result;
}

range parse_range(const std::string& r) {
  std::string str = remove_whitespace(r);
  int l = strlen("bytes=");
  if (str.substr(0, l) != "bytes=") return {-1, -1};
  std::string n1, n2;
  int it = l;
  while (it < r.length() && str[it] != '-') n1 += str[it++];
  it++;
  while (it < r.length()) n2 += str[it++];
  auto begin = n1.empty() ? -1 : atoll(n1.c_str());
  auto end = n2.empty() ? -1 : atoll(n2.c_str());
  return {begin, end == -1 ? -1 : (end - begin + 1)};
}

std::string address(const std::string& url, uint16_t port) {
  const auto https = "https://";
  const auto http = "http://";
  std::string result = url;
  if ((url.substr(0, strlen(https)) == https && port != 443) ||
      (url.substr(0, strlen(http)) == http && port != 80))
    result += ":" + std::to_string(port);
  return result;
}

std::string to_mime_type(const std::string& extension) {
  auto it = MIME_TYPE.find(to_lower(extension));
  if (it == std::end(MIME_TYPE))
    return "application/octet-stream";
  else
    return it->second;
}

}  // namespace util

}  // namespace cloudstorage
