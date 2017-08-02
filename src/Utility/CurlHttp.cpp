/*****************************************************************************
 * CurlHttp.cpp : implementation of CurlHttp
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

#include "CurlHttp.h"

#include <json/json.h>
#include <array>
#include <sstream>

#include "Utility.h"

const uint32_t MAX_URL_LENGTH = 1024;

namespace cloudstorage {

namespace {

struct write_callback_data {
  CURL* handle_;
  std::ostream* stream_;
  std::ostream* error_stream_;
  std::shared_ptr<CurlHttpRequest::ICallback> callback_;
  bool first_call_;
  bool success_;
};

size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
  write_callback_data* data = static_cast<write_callback_data*>(userdata);
  if (data->first_call_) {
    data->first_call_ = false;
    if (data->callback_) {
      long http_code = 0;
      curl_easy_getinfo(data->handle_, CURLINFO_RESPONSE_CODE, &http_code);
      data->callback_->receivedHttpCode(static_cast<int>(http_code));
      data->success_ = IHttpRequest::isSuccess(static_cast<int>(http_code));
      double content_length = 0;
      curl_easy_getinfo(data->handle_, CURLINFO_CONTENT_LENGTH_DOWNLOAD,
                        &content_length);
      data->callback_->receivedContentLength(static_cast<int>(content_length));
    }
  }
  if (!data->error_stream_ || data->success_)
    data->stream_->write(ptr, static_cast<std::streamsize>(size * nmemb));
  else
    data->error_stream_->write(ptr, static_cast<std::streamsize>(size * nmemb));
  return size * nmemb;
}

size_t read_callback(char* buffer, size_t size, size_t nmemb, void* userdata) {
  std::istream* stream = static_cast<std::istream*>(userdata);
  stream->read(buffer, size * nmemb);
  return stream->gcount();
}

int progress_callback(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
                      curl_off_t ultotal, curl_off_t ulnow) {
  CurlHttpRequest::ICallback* callback =
      static_cast<CurlHttpRequest::ICallback*>(clientp);
  if (callback) {
    if (ultotal != 0)
      callback->progressUpload(static_cast<uint32_t>(ultotal),
                               static_cast<uint32_t>(ulnow));
    if (dltotal != 0)
      callback->progressDownload(static_cast<uint32_t>(dltotal),
                                 static_cast<uint32_t>(dlnow));
    if (callback->abort()) return 1;
  }
  return 0;
}

std::ios::pos_type stream_length(std::istream& data) {
  data.seekg(0, data.end);
  std::ios::pos_type length = data.tellg();
  data.seekg(0, data.beg);
  return length;
}

}  // namespace

CurlHttpRequest::CurlHttpRequest(const std::string& url,
                                 const std::string& method,
                                 bool follow_redirect)
    : handle_(curl_easy_init()),
      url_(url),
      method_(method),
      follow_redirect_(follow_redirect) {
  curl_easy_setopt(handle_.get(), CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(handle_.get(), CURLOPT_READFUNCTION, read_callback);
  curl_easy_setopt(handle_.get(), CURLOPT_SSL_VERIFYPEER,
                   static_cast<long>(false));
  curl_easy_setopt(handle_.get(), CURLOPT_FOLLOWLOCATION,
                   static_cast<long>(follow_redirect));
  curl_easy_setopt(handle_.get(), CURLOPT_XFERINFOFUNCTION, progress_callback);
  curl_easy_setopt(handle_.get(), CURLOPT_NOPROGRESS, static_cast<long>(false));
}

void CurlHttpRequest::setParameter(const std::string& parameter,
                                   const std::string& value) {
  parameters_[parameter] = value;
}

void CurlHttpRequest::setHeaderParameter(const std::string& parameter,
                                         const std::string& value) {
  header_parameters_[parameter] = value;
}

const std::unordered_map<std::string, std::string>&
CurlHttpRequest::parameters() const {
  return parameters_;
}

const std::unordered_map<std::string, std::string>&
CurlHttpRequest::headerParameters() const {
  return header_parameters_;
}

bool CurlHttpRequest::follow_redirect() const { return follow_redirect_; }

const std::string& CurlHttpRequest::url() const { return url_; }

const std::string& CurlHttpRequest::method() const { return method_; }

int CurlHttpRequest::send(std::istream& data, std::ostream& response,
                          std::ostream* error_stream,
                          ICallback::Pointer p) const {
  std::shared_ptr<ICallback> callback(std::move(p));
  write_callback_data cb_data = {handle_.get(), &response, error_stream,
                                 callback,      true,      false};
  curl_easy_setopt(handle_.get(), CURLOPT_WRITEDATA, &cb_data);
  curl_slist* header_list = headerParametersToList();
  curl_easy_setopt(handle_.get(), CURLOPT_HTTPHEADER, header_list);
  std::string parameters = parametersToString();
  std::string url = url_ + (!parameters.empty() ? ("?" + parameters) : "");
  curl_easy_setopt(handle_.get(), CURLOPT_URL, url.c_str());
  curl_easy_setopt(handle_.get(), CURLOPT_XFERINFODATA, callback.get());
  curl_easy_setopt(handle_.get(), CURLOPT_READDATA, &data);
  CURLcode status = CURLE_OK;
  if (method_ == "POST") {
    curl_easy_setopt(handle_.get(), CURLOPT_POST, static_cast<long>(true));
    curl_easy_setopt(handle_.get(), CURLOPT_POSTFIELDSIZE,
                     static_cast<long>(stream_length(data)));
  } else if (method_ == "PUT") {
    curl_easy_setopt(handle_.get(), CURLOPT_UPLOAD, static_cast<long>(true));
    curl_easy_setopt(handle_.get(), CURLOPT_INFILESIZE,
                     static_cast<long>(stream_length(data)));
  } else if (method_ != "GET") {
    if (stream_length(data) > 0)
      curl_easy_setopt(handle_.get(), CURLOPT_UPLOAD, static_cast<long>(true));
    curl_easy_setopt(handle_.get(), CURLOPT_CUSTOMREQUEST, method_.c_str());
  }
  status = curl_easy_perform(handle_.get());
  curl_slist_free_all(header_list);
  if (status == CURLE_ABORTED_BY_CALLBACK)
    return Aborted;
  else if (status == CURLE_OK) {
    long http_code = static_cast<long>(Unknown);
    curl_easy_getinfo(handle_.get(), CURLINFO_RESPONSE_CODE, &http_code);
    if (!follow_redirect() && isRedirect(http_code)) {
      std::array<char, MAX_URL_LENGTH> redirect_url;
      char* data = redirect_url.data();
      curl_easy_getinfo(handle_.get(), CURLINFO_REDIRECT_URL, &data);
      response << data;
    }
    return static_cast<int>(http_code);
  } else
    return -status;
}

std::string CurlHttpRequest::parametersToString() const {
  std::string result;
  bool first = false;
  for (std::pair<std::string, std::string> p : parameters_) {
    if (first)
      result += "&";
    else
      first = true;
    result += p.first + "=" + p.second;
  }
  return result;
}

curl_slist* CurlHttpRequest::headerParametersToList() const {
  curl_slist* list = nullptr;
  for (std::pair<std::string, std::string> p : header_parameters_)
    list = curl_slist_append(list, (p.first + ": " + p.second).c_str());
  return list;
}

void CurlHttpRequest::CurlDeleter::operator()(CURL* handle) const {
  curl_easy_cleanup(handle);
}

IHttpRequest::Pointer CurlHttp::create(const std::string& url,
                                       const std::string& method,
                                       bool follow_redirect) const {
  return util::make_unique<CurlHttpRequest>(url, method, follow_redirect);
}

std::string CurlHttp::error(int code) const {
  return curl_easy_strerror(static_cast<CURLcode>(code));
}

}  // namespace cloudstorage
