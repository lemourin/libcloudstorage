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
#include <sstream>

namespace cloudstorage {

namespace {
size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
  std::ostream* stream = static_cast<std::ostream*>(userdata);
  *stream << std::string(ptr, ptr + size * nmemb);
  return size * nmemb;
}
}  // namespace

HttpRequest::HttpRequest(const std::string& url, Type t)
    : handle_(curl_easy_init(), CurlDeleter()), url_(url), type_(t) {
  curl_easy_setopt(handle_.get(), CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(handle_.get(), CURLOPT_SSL_VERIFYPEER, false);
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

const std::string& HttpRequest::post_data() const { return post_data_; }

void HttpRequest::set_post_data(const std::string& data) { post_data_ = data; }

HttpRequest::Type HttpRequest::type() const { return type_; }

void HttpRequest::set_type(HttpRequest::Type type) { type_ = type; }

std::string HttpRequest::send() const {
  std::stringstream stream;
  if (!send(stream)) throw std::runtime_error("Failed to send request.");
  return stream.str();
}

bool HttpRequest::send(std::ostream& response) const {
  curl_easy_setopt(handle_.get(), CURLOPT_WRITEDATA, &response);
  curl_slist* header_list = headerParametersToList();
  curl_easy_setopt(handle_.get(), CURLOPT_HTTPHEADER, header_list);
  bool success = true;
  if (type_ == Type::POST) {
    std::string post_data =
        post_data_.empty() ? parametersToString() : post_data_;
    curl_easy_setopt(handle_.get(), CURLOPT_URL, url_.c_str());
    curl_easy_setopt(handle_.get(), CURLOPT_POSTFIELDS, post_data.c_str());
    if (curl_easy_perform(handle_.get()) != CURLE_OK) success = false;
  } else if (type_ == Type::GET) {
    std::string parameters = parametersToString();
    std::string url = url_ + (!parameters.empty() ? ("?" + parameters) : "");
    curl_easy_setopt(handle_.get(), CURLOPT_URL, url.c_str());
    if (curl_easy_perform(handle_.get()) != CURLE_OK) success = false;
  }
  curl_slist_free_all(header_list);
  return success;
}

void HttpRequest::reset_parameters() {
  parameters_.clear();
  header_parameters_.clear();
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

bool HttpRequest::CurlDeleter::operator()(CURL* handle) const {
  curl_easy_cleanup(handle);
}

}  // namespace cloudstorage
