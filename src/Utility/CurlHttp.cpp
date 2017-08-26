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
const uint32_t POLL_TIMEOUT = 100;

namespace cloudstorage {

namespace curl {

namespace {

size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
  RequestData* data = static_cast<RequestData*>(userdata);
  if (data->first_call_) {
    data->first_call_ = false;
    if (data->callback_) {
      long http_code = 0;
      curl_easy_getinfo(data->handle_.get(), CURLINFO_RESPONSE_CODE,
                        &http_code);
      data->success_ = IHttpRequest::isSuccess(static_cast<int>(http_code));
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

CurlHttp::Worker::Worker() : done_(), thread_(std::bind(&Worker::work, this)) {}

CurlHttp::Worker::~Worker() {
  done_ = true;
  nonempty_.notify_one();
  thread_.join();
}

void CurlHttp::Worker::work() {
  auto handle = curl_multi_init();
  while (!done_ || !pending_.empty()) {
    std::unique_lock<std::mutex> lock(lock_);
    nonempty_.wait(lock, [=]() {
      return done_ || !requests_.empty() || !pending_.empty();
    });
    auto requests = std::move(requests_);
    lock.unlock();
    for (auto&& r : requests) {
      curl_multi_add_handle(handle, r->handle_.get());
      pending_[r->handle_.get()] = std::move(r);
    }
    int dummy;
    curl_multi_perform(handle, &dummy);
    curl_multi_wait(handle, nullptr, 0, POLL_TIMEOUT, &dummy);
    CURLMsg* msg;
    do {
      msg = curl_multi_info_read(handle, &dummy);
      if (msg && msg->msg == CURLMSG_DONE) {
        auto easy_handle = msg->easy_handle;
        curl_multi_remove_handle(handle, easy_handle);
        auto it = pending_.find(easy_handle);
        it->second->done(msg->data.result);
        pending_.erase(it);
      }
    } while (msg);
  }
  curl_multi_cleanup(handle);
}

void CurlHttp::Worker::add(RequestData::Pointer r) {
  {
    std::lock_guard<std::mutex> lock(lock_);
    requests_.push_back(std::move(r));
  }
  nonempty_.notify_all();
}

void RequestData::done(int code) {
  int ret = IHttpRequest::Unknown;
  uint64_t content_length = 0;
  if (code == CURLE_OK) {
    long http_code = static_cast<long>(IHttpRequest::Unknown);
    curl_easy_getinfo(handle_.get(), CURLINFO_RESPONSE_CODE, &http_code);
    ret = http_code;
    if (!follow_redirect_ && IHttpRequest::isRedirect(http_code)) {
      std::array<char, MAX_URL_LENGTH> redirect_url;
      char* data = redirect_url.data();
      curl_easy_getinfo(handle_.get(), CURLINFO_REDIRECT_URL, &data);
      *error_stream_ << data;
    }
    double curl_content_length;
    curl_easy_getinfo(handle_.get(), CURLINFO_CONTENT_LENGTH_DOWNLOAD,
                      &curl_content_length);
    content_length = (uint64_t)(curl_content_length + 0.5);
  } else {
    *error_stream_ << curl_easy_strerror(static_cast<CURLcode>(code));
    ret = (code == CURLE_ABORTED_BY_CALLBACK) ? IHttpRequest::Aborted : -code;
  }
  complete_({ret, content_length, stream_, error_stream_});
}

CurlHttpRequest::CurlHttpRequest(const std::string& url,
                                 const std::string& method,
                                 bool follow_redirect,
                                 std::shared_ptr<CurlHttp::Worker> worker)
    : url_(url),
      method_(method),
      follow_redirect_(follow_redirect),
      worker_(worker) {}

std::unique_ptr<CURL, CurlDeleter> CurlHttpRequest::init() const {
  std::unique_ptr<CURL, CurlDeleter> handle(curl_easy_init());
  curl_easy_setopt(handle.get(), CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(handle.get(), CURLOPT_READFUNCTION, read_callback);
  curl_easy_setopt(handle.get(), CURLOPT_SSL_VERIFYPEER,
                   static_cast<long>(false));
  curl_easy_setopt(handle.get(), CURLOPT_FOLLOWLOCATION,
                   static_cast<long>(follow_redirect_));
  curl_easy_setopt(handle.get(), CURLOPT_XFERINFOFUNCTION, progress_callback);
  curl_easy_setopt(handle.get(), CURLOPT_NOPROGRESS, static_cast<long>(false));
  std::string parameters = parametersToString();
  std::string url = url_ + (!parameters.empty() ? ("?" + parameters) : "");
  curl_easy_setopt(handle.get(), CURLOPT_URL, url.c_str());
  return handle;
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

RequestData::Pointer CurlHttpRequest::prepare(
    CompleteCallback complete, std::shared_ptr<std::istream> data,
    std::shared_ptr<std::ostream> response,
    std::shared_ptr<std::ostream> error_stream,
    ICallback::Pointer callback) const {
  auto cb_data = util::make_unique<RequestData>(RequestData{
      init(), headerParametersToList(), data, response, error_stream, callback,
      complete, follow_redirect(), true, false});
  auto handle = cb_data->handle_.get();
  curl_easy_setopt(handle, CURLOPT_WRITEDATA, cb_data.get());
  curl_easy_setopt(handle, CURLOPT_XFERINFODATA, callback.get());
  curl_easy_setopt(handle, CURLOPT_READDATA, data.get());
  curl_easy_setopt(handle, CURLOPT_HTTPHEADER, cb_data->headers_.get());
  if (method_ == "POST") {
    curl_easy_setopt(handle, CURLOPT_POST, static_cast<long>(true));
    curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE,
                     static_cast<long>(stream_length(*data)));
  } else if (method_ == "PUT") {
    curl_easy_setopt(handle, CURLOPT_UPLOAD, static_cast<long>(true));
    curl_easy_setopt(handle, CURLOPT_INFILESIZE,
                     static_cast<long>(stream_length(*data)));
  } else if (method_ == "HEAD") {
    curl_easy_setopt(handle, CURLOPT_NOBODY, 1L);
  } else if (method_ != "GET") {
    if (stream_length(*data) > 0)
      curl_easy_setopt(handle, CURLOPT_UPLOAD, static_cast<long>(true));
    curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, method_.c_str());
  }
  return cb_data;
}

void CurlHttpRequest::send(CompleteCallback c,
                           std::shared_ptr<std::istream> data,
                           std::shared_ptr<std::ostream> response,
                           std::shared_ptr<std::ostream> error_stream,
                           ICallback::Pointer cb) const {
  worker_->add(prepare(c, data, response, error_stream, cb));
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

std::unique_ptr<curl_slist, CurlListDeleter>
CurlHttpRequest::headerParametersToList() const {
  curl_slist* list = nullptr;
  for (std::pair<std::string, std::string> p : header_parameters_)
    list = curl_slist_append(list, (p.first + ": " + p.second).c_str());
  return std::unique_ptr<curl_slist, CurlListDeleter>(list);
}

void CurlDeleter::operator()(CURL* handle) const { curl_easy_cleanup(handle); }

void CurlListDeleter::operator()(curl_slist* lst) const {
  curl_slist_free_all(lst);
}

CurlHttp::CurlHttp() : worker_(std::make_shared<Worker>()) {}

IHttpRequest::Pointer CurlHttp::create(const std::string& url,
                                       const std::string& method,
                                       bool follow_redirect) const {
  return util::make_unique<CurlHttpRequest>(url, method, follow_redirect,
                                            worker_);
}

}  // namespace curl

}  // namespace cloudstorage
