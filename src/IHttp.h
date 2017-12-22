/*****************************************************************************
 * IHttp : IHttp interface
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

#ifndef IHTTP_H
#define IHTTP_H

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace cloudstorage {

class IHttpRequest {
 public:
  struct Response;

  using Pointer = std::shared_ptr<IHttpRequest>;
  using GetParameters = std::unordered_map<std::string, std::string>;
  using HeaderParameters = std::unordered_map<std::string, std::string>;
  using CompleteCallback = std::function<void(Response)>;

  struct Response {
    int http_code_;
    HeaderParameters headers_;  // header names should be lower cased
    std::shared_ptr<std::ostream> output_stream_;
    std::shared_ptr<std::ostream> error_stream_;
  };

  static constexpr int Ok = 200;
  static constexpr int Accepted = 202;
  static constexpr int Partial = 206;
  static constexpr int MultiStatus = 207;
  static constexpr int PermamentRedirect = 301;
  static constexpr int Bad = 400;
  static constexpr int Unauthorized = 401;
  static constexpr int Forbidden = 403;
  static constexpr int NotFound = 404;
  static constexpr int RangeInvalid = 416;
  static constexpr int ServiceUnavailable = 503;
  static constexpr int Aborted = 600;
  static constexpr int Unknown = 700;
  static constexpr int Failure = 800;

  virtual ~IHttpRequest() = default;

  class ICallback {
   public:
    using Pointer = std::shared_ptr<ICallback>;

    virtual ~ICallback() = default;

    virtual bool isSuccess(int code, const HeaderParameters&) const = 0;

    /**
     * @return whether to abort request right now
     */
    virtual bool abort() = 0;

    /**
     * Called when download progress changed.
     *
     * @param total count of bytes to download
     * @param now count of bytes downloaded
     */
    virtual void progressDownload(uint64_t total, uint64_t now) = 0;

    /**
     * Called when upload progress changed.
     *
     * @param total count of bytes to upload
     * @param now count of bytes uploaded
     */
    virtual void progressUpload(uint64_t total, uint64_t now) = 0;
  };

  /**
   * Sets GET parameter which will be appended to the url.
   *
   * @param parameter
   * @param value
   */
  virtual void setParameter(const std::string& parameter,
                            const std::string& value) = 0;

  /**
   * Sets http header parameter.
   *
   * @param parameter
   * @param value
   */
  virtual void setHeaderParameter(const std::string& parameter,
                                  const std::string& value) = 0;

  /**
   * Returns GET parameters set with setParameter.
   *
   * @return map of parameters
   */
  virtual const GetParameters& parameters() const = 0;

  /**
   * Returns header parameters set with setHeaderParameter.
   *
   * @return header parameters
   */
  virtual const HeaderParameters& headerParameters() const = 0;

  /**
   * @return url(without parameters set with setParameter)
   */
  virtual const std::string& url() const = 0;

  /**
   * @return method e.g GET, POST, DELETE, PATCH, ...
   */
  virtual const std::string& method() const = 0;

  /**
   * @return whether request follows redirections
   */
  virtual bool follow_redirect() const = 0;

  /**
   * Sends http request in an asynchronous fashion.
   *
   * @param on_completed callback to be called when request is finished
   *
   * @param data data to be sent in the request body
   *
   * @param response stream with server's response
   *
   * @param error_stream if set, response will be redirected here if http
   * request wasn't successful
   *
   */
  virtual void send(CompleteCallback on_completed,
                    std::shared_ptr<std::istream> data,
                    std::shared_ptr<std::ostream> response,
                    std::shared_ptr<std::ostream> error_stream = nullptr,
                    ICallback::Pointer = nullptr) const = 0;

  static bool isSuccess(int code) {
    return code / 100 == 2 || isRedirect(code);
  }
  static bool isRedirect(int code) { return code / 100 == 3; }
  static bool isClientError(int code) {
    return code / 100 == 4 || code / 100 == 5;
  }
  static bool isAuthorizationError(int code) { return code == 401; }
};

class IHttp {
 public:
  using Pointer = std::unique_ptr<IHttp>;

  virtual ~IHttp() = default;

  /**
   * Creates http request object.
   *
   * @param url
   * @param method
   * @param follow_redirect
   * @return http request
   */
  virtual IHttpRequest::Pointer create(const std::string& url,
                                       const std::string& method = "GET",
                                       bool follow_redirect = true) const = 0;

  static IHttp::Pointer create();
};

}  // namespace cloudstorage

#endif  // IHTTP_H
