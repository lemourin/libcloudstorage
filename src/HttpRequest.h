/*****************************************************************************
 * HttpRequest.h : interface of HttpRequest
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

#ifndef HTTPREQUEST_H
#define HTTPREQUEST_H

#include <curl/curl.h>
#include <memory>
#include <string>
#include <unordered_map>

namespace cloudstorage {

class HttpException : public std::exception {
 public:
  HttpException(CURLcode);

  CURLcode code() const { return code_; }
  const char* what() const noexcept { return description_.c_str(); }

 private:
  CURLcode code_;
  std::string description_;
};

class HttpRequest {
 public:
  using Pointer = std::unique_ptr<HttpRequest>;

  class ICallback {
   public:
    using Pointer = std::unique_ptr<ICallback>;
    virtual ~ICallback() = default;

    virtual bool abort() = 0;
    virtual void progressDownload(uint total, uint now) = 0;
    virtual void progressUpload(uint total, uint now) = 0;
    virtual void receivedHttpCode(int code) = 0;
    virtual void receivedContentLength(int length) = 0;
  };

  enum class Type { POST, GET, PUT };

  HttpRequest(const std::string& url, Type);

  void setParameter(const std::string& parameter, const std::string& value);
  void setHeaderParameter(const std::string& parameter,
                          const std::string& value);

  const std::string& url() const;
  void set_url(const std::string&);

  Type type() const;
  void set_type(Type);

  std::string send() const;
  std::string send(std::istream& data) const;
  int send(std::ostream& response) const;
  int send(std::istream& data, std::ostream& response,
           std::ostream* error_stream = nullptr,
           ICallback::Pointer = nullptr) const;

  void resetParameters();

  static bool isSuccess(int code);
  static bool isClientError(int code);

 private:
  struct CurlDeleter {
    void operator()(CURL*) const;
  };

  std::string parametersToString() const;
  curl_slist* headerParametersToList() const;

  std::unique_ptr<CURL, CurlDeleter> handle_;
  std::string url_;
  std::unordered_map<std::string, std::string> parameters_;
  std::unordered_map<std::string, std::string> header_parameters_;
  Type type_;
};

}  // namespace cloudstorage

#endif  // HTTPREQUEST_H
