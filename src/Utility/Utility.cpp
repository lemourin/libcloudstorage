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

namespace cloudstorage {

namespace util {

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

}  // namespace util

}  // namespace cloudstorage
