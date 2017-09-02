/*****************************************************************************
 * Utility.h : prototypes for utility functions
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

#ifndef UTILITY_H
#define UTILITY_H

#include <memory>
#include <string>

#include "IHttpServer.h"
#include "IItem.h"

namespace cloudstorage {

struct Range;

bool operator==(const Range&, const Range&);
bool operator!=(const Range&, const Range&);

namespace util {

using Output = std::shared_ptr<std::ostream>;
using Input = std::shared_ptr<std::istream>;

template <typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

std::string remove_whitespace(const std::string& str);
Range parse_range(const std::string& str);
std::string range_to_string(Range);
std::string address(const std::string& url, uint16_t port);
std::string to_mime_type(const std::string& extension);
IItem::TimeStamp parse_time(const std::string& time);
std::tm gmtime(time_t);
time_t timegm(const std::tm&);
std::string to_base64(const std::string&);
std::string from_base64(const std::string&);

class Url {
 public:
  Url(const std::string&);

  static std::string unescape(const std::string&);
  static std::string escape(const std::string&);
  static std::string escapeHeader(const std::string&);

  std::string protocol() const { return protocol_; }
  std::string host() const { return host_; }
  std::string path() const { return path_; }
  std::string query() const { return query_; }

 private:
  std::string protocol_;
  std::string host_;
  std::string path_;
  std::string query_;
};

IHttpServer::IResponse::Pointer response_from_string(
    const IHttpServer::IRequest&, int code,
    const IHttpServer::IResponse::Headers&, const std::string&);

}  // namespace util

}  // namespace cloudstorage

#endif  // UTILITY_H
