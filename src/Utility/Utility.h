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

#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>

#include "IHttpServer.h"
#include "IItem.h"

namespace Json {
class Value;
}  // namespace Json

namespace cloudstorage {

struct Range;

bool operator==(const Range&, const Range&);
bool operator!=(const Range&, const Range&);

namespace util {

using Output = std::shared_ptr<std::ostream>;
using Input = std::shared_ptr<std::istream>;

struct FileId {
  FileId(bool, const std::string&);
  FileId(const std::string&);

  operator std::string() const;

  bool folder_;
  std::string id_;
};

template <typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

template <class T, class U = T>
T exchange(T& obj, U&& new_value) {
  T old_value = std::move(obj);
  obj = std::forward<U>(new_value);
  return old_value;
}

std::string to_lower(std::string);
std::string remove_whitespace(const std::string& str);
Range parse_range(const std::string& str);
std::string range_to_string(Range);
std::string to_mime_type(const std::string& extension);
IItem::TimeStamp parse_time(const std::string& time);
std::tm gmtime(time_t);
time_t timegm(const std::tm&);
std::string to_base64(const std::string&);
std::string from_base64(const std::string&);
std::string login_page(const std::string& provider);
std::string success_page(const std::string& provider);
std::string error_page(const std::string& provider);
namespace json {
std::string to_string(const Json::Value&);
Json::Value from_stream(std::istream&&);
Json::Value from_stream(std::istream&);
}  // namespace json
const char* libcloudstorage_ascii_art();

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

std::string temporary_directory();
std::string home_directory();

namespace priv {
extern std::mutex stream_mutex;
extern std::unique_ptr<std::ostream> stream;

template <class... Args>
void log() {}

template <class First, class... Rest>
void log(First&& t, Rest&&... rest) {
  *priv::stream << t << " ";
  log(std::forward<Rest>(rest)...);
}

}  // namespace priv

void log_stream(std::unique_ptr<std::ostream> stream);

template <class... Args>
void log(Args&&... t) {
  std::lock_guard<std::mutex> lock(priv::stream_mutex);
  std::time_t time = std::time(nullptr);
  auto tm = util::gmtime(time);
  *priv::stream << "[" << std::put_time(&tm, "%D %T") << "] ";
  priv::log(std::forward<Args>(t)...);
  *priv::stream << std::endl;
}

}  // namespace util

}  // namespace cloudstorage

#endif  // UTILITY_H
