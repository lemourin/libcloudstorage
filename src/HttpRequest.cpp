/*****************************************************************************
 * HttpRequest.cpp : implementation of HttpRequest
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

#include "HttpRequest.h"
#include <iostream>
#include <sstream>

namespace cloudstorage {

namespace {
size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
  std::ostream* stream = static_cast<std::ostream*>(userdata);
  stream->write(ptr, size * nmemb);
  return size * nmemb;
}

size_t read_callback(char* buffer, size_t size, size_t nmemb, void* userdata) {
  std::istream* stream = static_cast<std::istream*>(userdata);
  stream->read(buffer, size * nmemb);
  return stream->gcount();
}

std::ios::pos_type stream_length(std::istream& data) {
  data.seekg(0, data.end);
  std::ios::pos_type length = data.tellg();
  data.seekg(0, data.beg);
  return length;
}

}  // namespace

HttpRequest::HttpRequest(const std::string& url, Type t)
    : handle_(curl_easy_init(), CurlDeleter()), url_(url), type_(t) {
  curl_easy_setopt(handle_.get(), CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(handle_.get(), CURLOPT_READFUNCTION, read_callback);
  curl_easy_setopt(handle_.get(), CURLOPT_SSL_VERIFYPEER, false);
  curl_easy_setopt(handle_.get(), CURLOPT_FOLLOWLOCATION, true);
}

void HttpRequest::setParameter(const std::string& parameter,
                               const std::string& value) {
  parameters_[parameter] = value;
}

void HttpRequest::setHeaderParameter(const std::string& parameter,
                                     const std::string& value) {
  header_parameters_[parameter] = value;
}

const std::string& HttpRequest::url() const { return url_; }

void HttpRequest::set_url(const std::string& url) { url_ = url; }

HttpRequest::Type HttpRequest::type() const { return type_; }

void HttpRequest::set_type(HttpRequest::Type type) { type_ = type; }

std::string HttpRequest::send() const {
  std::stringstream data, response;
  if (!send(data, response))
    throw std::runtime_error("Failed to send request.");
  return response.str();
}

std::string HttpRequest::send(std::istream& data) const {
  std::stringstream response;
  if (!send(data, response))
    throw std::runtime_error("Failed to send request.");
  return response.str();
}

int HttpRequest::send(std::ostream& response) const {
  std::stringstream data;
  return send(data, response);
}

int HttpRequest::send(std::istream& data, std::ostream& response) const {
  curl_easy_setopt(handle_.get(), CURLOPT_WRITEDATA, &response);
  curl_slist* header_list = headerParametersToList();
  curl_easy_setopt(handle_.get(), CURLOPT_HTTPHEADER, header_list);
  std::string parameters = parametersToString();
  std::string url = url_ + (!parameters.empty() ? ("?" + parameters) : "");
  curl_easy_setopt(handle_.get(), CURLOPT_URL, url.c_str());
  bool success = true;
  if (type_ == Type::POST) {
    curl_easy_setopt(handle_.get(), CURLOPT_POST, true);
    curl_easy_setopt(handle_.get(), CURLOPT_READDATA, &data);
    curl_easy_setopt(handle_.get(), CURLOPT_POSTFIELDSIZE_LARGE,
                     stream_length(data));
    if (curl_easy_perform(handle_.get()) != CURLE_OK) success = false;
  } else if (type_ == Type::GET) {
    if (curl_easy_perform(handle_.get()) != CURLE_OK) success = false;
  } else if (type_ == Type::PUT) {
    curl_easy_setopt(handle_.get(), CURLOPT_PUT, true);
    curl_easy_setopt(handle_.get(), CURLOPT_READDATA, &data);
    curl_easy_setopt(handle_.get(), CURLOPT_INFILESIZE_LARGE,
                     stream_length(data));
    if (curl_easy_perform(handle_.get()) != CURLE_OK) success = false;
  }
  int return_code = 0;
  if (success) {
    long http_code = 0;
    curl_easy_getinfo(handle_.get(), CURLINFO_RESPONSE_CODE, &http_code);
    return_code = http_code;
  }
  curl_slist_free_all(header_list);
  return return_code;
}

void HttpRequest::resetParameters() {
  parameters_.clear();
  header_parameters_.clear();
}

bool HttpRequest::isSuccess(int code) {
  return (code / 100) == 2;
}

std::string HttpRequest::parametersToString() const {
  std::string result;
  for (std::pair<std::string, std::string> p : parameters_)
    result += p.first + "=" + p.second + "&";
  return result;
}

curl_slist* HttpRequest::headerParametersToList() const {
  curl_slist* list = nullptr;
  for (std::pair<std::string, std::string> p : header_parameters_)
    list = curl_slist_append(list, (p.first + ": " + p.second).c_str());
  return list;
}

void HttpRequest::CurlDeleter::operator()(CURL* handle) const {
  curl_easy_cleanup(handle);
}

}  // namespace cloudstorage
