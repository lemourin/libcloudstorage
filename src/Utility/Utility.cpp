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

#include "IItem.h"
#include "IRequest.h"

#include <json/json.h>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <unordered_map>

#ifdef _WIN32
#include <windows.h>

#if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
#define WINRT
#endif

#endif

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

const std::string CDN =
    "<script src='https://code.jquery.com/jquery-3.1.0.min.js'"
    "integrity='sha256-cCueBR6CsyA4/9szpPfrX3s49M9vUU5BgtiJj06wt/s='"
    "crossorigin='anonymous'></script>"
    "<script "
    "src='https://cdnjs.cloudflare.com/ajax/libs/js-url/2.5.0/url.min.js'>"
    "</script>";

namespace {

unsigned char from_hex(unsigned char ch) {
  if (ch <= '9' && ch >= '0')
    ch -= '0';
  else if (ch <= 'f' && ch >= 'a')
    ch -= 'a' - 10;
  else if (ch <= 'F' && ch >= 'A')
    ch -= 'A' - 10;
  else
    ch = 0;
  return ch;
}

bool leap_year(int year) {
  return !(year % 4) && ((year % 100) || !(year % 400));
}

int year_size(int year) { return leap_year(year) ? 366 : 365; }

}  // namespace

bool operator==(const Range& r1, const Range& r2) {
  return std::tie(r1.start_, r1.size_) == std::tie(r2.start_, r2.size_);
}

bool operator!=(const Range& r1, const Range& r2) { return !(r1 == r2); }

namespace util {

namespace priv {
std::unique_ptr<std::ostream> stream =
    util::make_unique<std::ostream>(std::cout.rdbuf());
std::mutex stream_mutex;
}  // namespace priv

FileId::FileId(bool folder, const std::string& id) : folder_(folder), id_(id) {}

FileId::FileId(const std::string& str) : folder_() {
  try {
    auto json = json::from_stream(std::stringstream(util::from_base64(str)));
    folder_ = json["t"].asBool();
    id_ = json["id"].asString();
  } catch (const Json::Exception&) {
  }
}

FileId::operator std::string() const {
  Json::Value json;
  json["t"] = folder_;
  json["id"] = id_;
  return util::to_base64(json::to_string(json));
}

std::string to_lower(std::string str) {
  for (char& c : str) c = tolower(c);
  return str;
}

std::string remove_whitespace(const std::string& str) {
  std::string result;
  for (char c : str)
    if (!isspace(c)) result += c;
  return result;
}

Range parse_range(const std::string& r) {
  std::string str = remove_whitespace(r);
  size_t l = strlen("bytes=");
  if (str.substr(0, l) != "bytes=") return {0, 0};
  std::string n1, n2;
  size_t it = l;
  while (it < r.length() && str[it] != '-') n1 += str[it++];
  it++;
  while (it < r.length()) n2 += str[it++];
  auto begin = n1.empty() ? 0 : stoull(n1);
  auto end = n2.empty() ? Range::Full : stoull(n2);
  return {begin, end == Range::Full ? Range::Full : (end - begin + 1)};
}

std::string range_to_string(Range r) {
  std::stringstream stream;
  stream << "bytes=" << r.start_ << "-";
  if (r.size_ != Range::Full) stream << r.size_ + r.start_ - 1;
  return stream.str();
}

std::string to_mime_type(const std::string& extension) {
  auto it = MIME_TYPE.find(to_lower(extension));
  if (it == std::end(MIME_TYPE))
    return "application/octet-stream";
  else
    return it->second;
}

std::tm gmtime(time_t time) {
  const int ytab[2][12] = {{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
                           {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}};
  auto year = 1970;
  auto dayclock = time % (24 * 60 * 60);
  auto dayno = time / (24 * 60 * 60);
  std::tm tmbuf = {};

  tmbuf.tm_sec = dayclock % 60;
  tmbuf.tm_min = (dayclock % 3600) / 60;
  tmbuf.tm_hour = dayclock / 3600;
  tmbuf.tm_wday = (dayno + 4) % 7;
  while (dayno >= year_size(year)) {
    dayno -= year_size(year);
    year++;
  }
  tmbuf.tm_year = year - 1900;
  tmbuf.tm_yday = dayno;
  while (dayno >= ytab[leap_year(year)][tmbuf.tm_mon]) {
    dayno -= ytab[leap_year(year)][tmbuf.tm_mon];
    tmbuf.tm_mon++;
  }
  tmbuf.tm_mday = dayno + 1;
  return tmbuf;
}

time_t timegm(const std::tm& t) {
  const int month_count = 12;
  const int days[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
  int year = 1900 + t.tm_year + t.tm_mon / month_count;
  time_t result = (year - 1970) * 365 + days[t.tm_mon % month_count];

  result += (year - 1968) / 4;
  result -= (year - 1900) / 100;
  result += (year - 1600) / 400;
  if ((year % 4) == 0 && ((year % 100) != 0 || (year % 400) == 0) &&
      (t.tm_mon % month_count) < 2)
    result--;
  result += t.tm_mday - 1;
  result *= 24;
  result += t.tm_hour;
  result *= 60;
  result += t.tm_min;
  result *= 60;
  result += t.tm_sec;
  if (t.tm_isdst == 1) result -= 3600;
  return result;
}

IItem::TimeStamp parse_time(const std::string& str) {
  const uint32_t SIZE = 6;
  char buffer[SIZE + 1] = {};
  float sec;
  std::tm time = {};
  if (sscanf(str.c_str(), "%d-%d-%dT%d:%d:%f%6s", &time.tm_year, &time.tm_mon,
             &time.tm_mday, &time.tm_hour, &time.tm_min, &sec, buffer) == 7) {
    time.tm_year -= 1900;
    time.tm_mon--;
    time.tm_sec = (int)(sec + 0.5);
    if (buffer != std::string("Z")) {
      int offset_hour, offset_minute;
      if (sscanf(buffer, "%d:%d", &offset_hour, &offset_minute) == 2) {
        time.tm_hour -= offset_hour;
        time.tm_min -= offset_minute;
      }
    }
    return std::chrono::system_clock::time_point(
        std::chrono::seconds(timegm(time)));
  }
  return IItem::UnknownTimeStamp;
}

std::string to_base64(const std::string& in) {
  const char* base64_chars =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  int val = 0, valb = -6;
  for (uint8_t c : in) {
    val = (val << 8) + c;
    valb += 8;
    while (valb >= 0) {
      out.push_back(base64_chars[(val >> valb) & 0x3F]);
      valb -= 6;
    }
  }
  if (valb > -6) out.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
  while (out.size() % 4) out.push_back('=');
  return out;
}

std::string from_base64(const std::string& in) {
  const uint8_t lookup[] = {
      62,  255, 62,  255, 63,  52,  53, 54, 55, 56, 57, 58, 59, 60, 61, 255,
      255, 0,   255, 255, 255, 255, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
      10,  11,  12,  13,  14,  15,  16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
      255, 255, 255, 255, 63,  255, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,
      36,  37,  38,  39,  40,  41,  42, 43, 44, 45, 46, 47, 48, 49, 50, 51};

  std::string out;
  int val = 0, valb = -8;
  for (uint8_t c : in) {
    if (c < '+' || c > 'z') break;
    c -= '+';
    if (lookup[c] >= 64) break;
    val = (val << 6) + lookup[c];
    valb += 6;
    if (valb >= 0) {
      out.push_back(char((val >> valb) & 0xFF));
      valb -= 8;
    }
  }
  return out;
}

Url::Url(const std::string& url) {
  const std::string prot_end = "://";

  auto it =
      std::search(url.begin(), url.end(), prot_end.begin(), prot_end.end());
  if (it == url.end()) {
    it = url.begin();
    data_.protocol_ = "http";
  } else {
    data_.protocol_ = std::string(url.begin(), it);
    it += prot_end.length();
  }

  auto it_next = std::find(it, url.end(), '/');
  data_.host_ = std::string(it, it_next);
  it = it_next;

  it_next = std::find(it, url.end(), '?');
  data_.path_ = std::string(it, it_next);
  it = it_next;

  if (it != url.end()) it++;
  data_.query_ = std::string(it, url.end());
}

std::string Url::unescape(const std::string& str) {
  std::string result;
  for (size_t i = 0; i < str.size(); ++i) {
    if (str[i] == '+') {
      result += ' ';
    } else if (str[i] == '%' && str.size() > i + 2) {
      auto ch1 = from_hex(str[i + 1]);
      auto ch2 = from_hex(str[i + 2]);
      auto ch = (ch1 << 4) | ch2;
      result += ch;
      i += 2;
    } else {
      result += str[i];
    }
  }
  return result;
}

std::string Url::escape(const std::string& value) {
  std::ostringstream escaped;
  escaped << std::setfill('0');
  escaped << std::hex;

  for (std::string::const_iterator i = value.begin(); i != value.end(); i++) {
    std::string::value_type c = *i;
    if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      escaped << c;
      continue;
    }
    escaped << std::uppercase;
    escaped << '%' << std::setw(2) << int(static_cast<unsigned char>(c));
    escaped << std::nouppercase;
  }
  return escaped.str();
}

std::string Url::escapeHeader(const std::string& header) {
  return Json::valueToQuotedString(header.c_str());
}

std::string Url::protocol() const { return data_.protocol_; }

std::string Url::host() const { return data_.host_; }

std::string Url::path() const { return data_.path_; }

std::string Url::query() const { return data_.query_; }

IHttpServer::IResponse::Pointer response_from_string(
    const IHttpServer::IRequest& request, int code,
    const IHttpServer::IResponse::Headers& headers, const std::string& data) {
  class DataProvider : public IHttpServer::IResponse::ICallback {
   public:
    DataProvider(const std::string& data) : position_(), data_(data) {}

    int putData(char* buffer, size_t max) override {
      int cnt = std::min<int>(data_.length() - position_, max);
      memcpy(buffer, data_.data() + position_, cnt);
      position_ += cnt;
      return cnt;
    }

   private:
    int position_;
    std::string data_;
  };
  return request.response(code, headers, data.length(),
                          util::make_unique<DataProvider>(data));
}

std::string temporary_directory() {
#ifdef WINRT
  return ".\\";
#elif _WIN32
  const char* temp = getenv("TEMP");
  return temp ? std::string(temp) + "\\" : ".\\";
#else
  return "/tmp/";
#endif
}

std::string home_directory() {
#ifdef WINRT
  return ".";
#elif _WIN32
  const char* drive = getenv("Homedrive");
  const char* path = getenv("Homepath");
  return (drive && path) ? std::string(drive) + path : ".";
#elif __ANDROID__
  return "/storage/emulated/0";
#else
  const char* home = getenv("HOME");
  return home ? home : ".";
#endif
}

std::string login_page(const std::string&) {
  return "<html>" + CDN +
         "<body>"
         "libcloudstorage login page"
         "<table>"
         "<tr><td>Username:</td><td><input id='username'/></td></tr>"
         "<tr><td>Password:</td><td><input id='password' "
         "type='password'/></td></tr>"
         "<tr><td>AmazonS3 bucket:</td><td><input id='bucket'/></td></tr>"
         "<tr><td>WebDAV url:</td><td><input id='webdav_url'/></td></tr>"
         "<tr><td>Local drive path:</td><td><input id='path'/></td></tr>"
         "<tr><td>"
         "  <a id='link'><input id='submit' type='button' value='Login'></a>"
         "</td></tr>"
         "<script>"
         " $(function() {"
         "   var func = function() {"
         "     var json = {"
         "        username: $('#username').val(),"
         "        password: $('#password').val(),"
         "        bucket: $('#bucket').val(),"
         "        webdav_url: $('#webdav_url').val(),"
         "        path: $('#path').val()"
         "     };"
         "     var str = window.btoa(encodeURIComponent(JSON.stringify(json)));"
         "     $('#link').attr('href', location.pathname + '?code='"
         "                     + encodeURIComponent(str) + "
         "                     '&state=' +  url('?').state);"
         "   };"
         "   $('#login').change(func);"
         "   $('#password').change(func);"
         "   $('#bucket').change(func);"
         "   $('#webdav_url').change(func);"
         "   $('#path').change(func);"
         " });"
         "</script>"
         "</table>"
         "</body>"
         "</html>";
}

std::string success_page(const std::string&) {
  return "<html>" + CDN +
         "<body>Success.</body>"
         "<script>"
         "  $.ajax({ 'data': { 'accepted': 'true' } });"
         "</script>"
         "</html>";
}

std::string error_page(const std::string&) {
  return "<html>" + CDN +
         "<body>Error.</body>"
         "<script>"
         "  $.ajax({ 'data': { 'accepted': 'false' } });"
         "</script>"
         "</html>";
}

std::string json::to_string(const Json::Value& json) {
  Json::StreamWriterBuilder stream;
  stream["indentation"] = "";
  return Json::writeString(stream, json);
}

Json::Value json::from_string(const std::string& str) {
  Json::CharReaderBuilder factory;
  std::unique_ptr<Json::CharReader> reader(factory.newCharReader());
  Json::Value json;
  std::string error;
  if (!reader->parse(str.data(), str.data() + str.size(), &json, &error))
    throw Json::Exception(error);
  return json;
}

Json::Value json::from_stream(std::istream&& stream) {
  return from_stream(static_cast<std::istream&>(stream));
}

Json::Value json::from_stream(std::istream& stream) {
  std::stringstream sstream;
  sstream << stream.rdbuf();
  return from_string(sstream.str());
}

const char* libcloudstorage_ascii_art() {
  return R"(   _ _ _          _                 _     _                             
  | (_| |        | |               | |   | |                            
  | |_| |__   ___| | ___  _   _  __| |___| |_ ___  _ __ __ _  __ _  ___ 
  | | | '_ \ / __| |/ _ \| | | |/ _` / __| __/ _ \| '__/ _` |/ _` |/ _ \
  | | | |_) | (__| | (_) | |_| | (_| \__ | || (_) | | | (_| | (_| |  __/
  |_|_|_.__/ \___|_|\___/ \__,_|\__,_|___/\__\___/|_|  \__,_|\__, |\___|
                                                              __/ |     
                                                             |___/      )";
}

void log_stream(std::unique_ptr<std::ostream> stream) {
  std::lock_guard<std::mutex> lock(priv::stream_mutex);
  priv::stream = std::move(stream);
}

}  // namespace util
}  // namespace cloudstorage
