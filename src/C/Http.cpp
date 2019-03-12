/*****************************************************************************
 * Http.cpp
 *
 *****************************************************************************
 * Copyright (C) 2016-2018 VideoLAN
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
#include "Http.h"

#include "IHttp.h"
#include "Utility/Utility.h"

using namespace cloudstorage;

cloud_http *cloud_http_create_default() {
  return reinterpret_cast<cloud_http *>(IHttp::create().release());
}

cloud_http *cloud_http_create(cloud_http_operations *ops, void *userdata) {
  struct HttpRequest : public IHttpRequest {
    HttpRequest(cloud_http_operations ops, cloud_http_request *request,
                const std::string &url, const std::string &method,
                bool follow_redirect)
        : operations_(ops),
          request_(request),
          url_(url),
          method_(method),
          follow_redirect_(follow_redirect) {}

    ~HttpRequest() override { operations_.release_http_request(request_); }

    void setParameter(const std::string &parameter,
                      const std::string &value) override {
      parameters_.insert({parameter, value});
      operations_.set_parameter(request_, parameter.c_str(), value.c_str());
    }

    void setHeaderParameter(const std::string &parameter,
                            const std::string &value) override {
      header_paremeters_.insert({parameter, value});
      operations_.set_header_parameter(request_, parameter.c_str(),
                                       value.c_str());
    }

    const GetParameters &parameters() const override { return parameters_; }

    const HeaderParameters &headerParameters() const override {
      return header_paremeters_;
    }

    const std::string &url() const override { return url_; }

    const std::string &method() const override { return method_; }

    bool follow_redirect() const override { return follow_redirect_; }

    void send(CompleteCallback on_completed, std::shared_ptr<std::istream> data,
              std::shared_ptr<std::ostream> response,
              std::shared_ptr<std::ostream> error_stream,
              ICallback::Pointer callback) const override {
      struct CallbackData {
        CompleteCallback on_completed;
        std::shared_ptr<std::istream> data_;
        std::shared_ptr<std::ostream> response_;
        std::shared_ptr<std::ostream> error_stream_;
        ICallback::Pointer callback_;
      } *callback_data = new CallbackData{on_completed, data, response,
                                          error_stream, callback};

      operations_.send(
          request_,
          [](const void *param, struct cloud_http_response *r) {
            auto data = reinterpret_cast<const CallbackData *>(param);
            IHttpRequest::Response response;
            response.http_code_ = r->http_code;
            response.headers_ =
                *reinterpret_cast<HttpRequest::HeaderParameters *>(r->headers);
            response.output_stream_ = data->response_;
            response.error_stream_ = data->error_stream_;
            data->on_completed(response);
            delete data;
          },
          callback_data,
          reinterpret_cast<cloud_input *>(callback_data->data_.get()),
          reinterpret_cast<cloud_output *>(callback_data->response_.get()),
          reinterpret_cast<cloud_output *>(callback_data->error_stream_.get()),
          reinterpret_cast<cloud_http_callback *>(
              callback_data->callback_.get()));
    }

    cloud_http_operations operations_;
    cloud_http_request *request_;
    std::string url_;
    std::string method_;
    HeaderParameters header_paremeters_;
    GetParameters parameters_;
    bool follow_redirect_;
  };

  struct Http : public IHttp {
    Http(cloud_http_operations ops, void *userdata)
        : operations_(ops), userdata_(userdata) {}

    ~Http() override { operations_.release(userdata_); }

    IHttpRequest::Pointer create(const std::string &url,
                                 const std::string &method,
                                 bool follow_redirect = true) const {
      return std::make_shared<HttpRequest>(
          operations_,
          operations_.create(url.c_str(), method.c_str(), follow_redirect,
                             userdata_),
          url, method, follow_redirect);
    }

    cloud_http_operations operations_;
    void *userdata_;
  };

  return reinterpret_cast<cloud_http *>(
      util::make_unique<Http>(*ops, userdata).release());
}

void cloud_http_release(cloud_http *d) { delete reinterpret_cast<IHttp *>(d); }

cloud_http_headers *cloud_http_headers_create() {
  return reinterpret_cast<cloud_http_headers *>(
      new IHttpRequest::HeaderParameters());
}

void cloud_http_headers_release(cloud_http_headers *d) {
  delete reinterpret_cast<IHttpRequest::HeaderParameters *>(d);
}

void cloud_http_headers_put(cloud_http_headers *h, cloud_string *key,
                            cloud_string *value) {
  auto d = reinterpret_cast<IHttpRequest::HeaderParameters *>(h);
  d->insert({key, value});
}

size_t cloud_input_size(cloud_input *i) {
  auto d = *reinterpret_cast<std::shared_ptr<std::istream> *>(i);
  d->seekg(0, d->end);
  auto length = d->tellg();
  d->seekg(0, d->beg);
  return length;
}

size_t cloud_input_read(cloud_input *i, uint8_t *data, size_t maxbytes) {
  auto d = reinterpret_cast<std::shared_ptr<std::istream> *>(i);
  (*d)->read(reinterpret_cast<char *>(data), maxbytes);
  return (*d)->eof() ? EOF : (*d)->gcount();
}

int cloud_output_write(cloud_output *o, uint8_t *data, size_t size) {
  auto d = reinterpret_cast<std::shared_ptr<std::ostream> *>(o);
  (*d)->write(reinterpret_cast<char *>(data), size);
  return (*d)->fail() ? -1 : 0;
}

int cloud_http_callback_is_success(cloud_http_callback *callback, int code,
                                   const cloud_http_headers *headers) {
  return reinterpret_cast<IHttpRequest::ICallback *>(callback)->isSuccess(
      code, *reinterpret_cast<const IHttpRequest::HeaderParameters *>(headers));
}

int cloud_http_callback_abort(cloud_http_callback *callback) {
  return reinterpret_cast<IHttpRequest::ICallback *>(callback)->abort();
}

int cloud_http_callback_pause(cloud_http_callback *callback) {
  return reinterpret_cast<IHttpRequest::ICallback *>(callback)->pause();
}

void cloud_http_callback_progress_download(cloud_http_callback *callback,
                                           uint64_t total, uint64_t now) {
  reinterpret_cast<IHttpRequest::ICallback *>(callback)->progressDownload(total,
                                                                          now);
}

void cloud_http_callback_progress_upload(cloud_http_callback *callback,
                                         uint64_t total, uint64_t now) {
  reinterpret_cast<IHttpRequest::ICallback *>(callback)->progressUpload(total,
                                                                        now);
}
