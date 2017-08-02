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

#include <memory>
#include <string>
#include <unordered_map>

namespace cloudstorage {

class IHttpRequest {
 public:
  using Pointer = std::shared_ptr<IHttpRequest>;

  static constexpr int Ok = 200;
  static constexpr int Partial = 206;
  static constexpr int Bad = 400;
  static constexpr int Unauthorized = 401;
  static constexpr int Forbidden = 403;
  static constexpr int NotFound = 404;
  static constexpr int RangeInvalid = 416;
  static constexpr int Aborted = 600;
  static constexpr int Unknown = 700;
  static constexpr int Failure = 800;

  class ICallback {
   public:
    using Pointer = std::unique_ptr<ICallback>;

    virtual ~ICallback() = default;

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
    virtual void progressDownload(uint32_t total, uint32_t now) = 0;

    /**
     * Called when upload progress changed.
     *
     * @param total count of bytes to upload
     * @param now count of bytes uploaded
     */
    virtual void progressUpload(uint32_t total, uint32_t now) = 0;

    /**
     * Called when response's http code was received.
     *
     * @param code received http code
     */
    virtual void receivedHttpCode(int code) = 0;

    /**
     * Called when response's Content-Length header was received.
     *
     * @param length Content-Length
     */
    virtual void receivedContentLength(int length) = 0;
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
  virtual const std::unordered_map<std::string, std::string>& parameters()
      const = 0;

  /**
   * Returns header parameters set with setHeaderParameter.
   *
   * @return header parameters
   */
  virtual const std::unordered_map<std::string, std::string>& headerParameters()
      const = 0;

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
   * Sends http request and blocks until it receives full response.
   *
   * @param data data to be sent in the request body
   *
   * @param response stream with server's response
   *
   * @param error_stream if set, response will be redirected here if http
   * request wasn't successful
   *
   * @return if < 0, that means a library error occured with code -(returned
   * value), otherwise http code of the response is returned, or one of values:
   * Unknown, Aborted
   */
  virtual int send(std::istream& data, std::ostream& response,
                   std::ostream* error_stream = nullptr,
                   ICallback::Pointer = nullptr) const = 0;

  static bool isSuccess(int code) { return code / 100 == 2; }
  static bool isRedirect(int code) { return code / 100 == 3; }
  static bool isClientError(int code) {
    return code / 100 == 4 || code / 100 == 5;
  }
  static bool isAuthorizationError(int code) { return code == 401; }
};

class IHttp {
 public:
  using Pointer = std::unique_ptr<IHttp>;

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

  /**
   * Gives description of http library's error code.
   *
   * @param code error code
   * @return error description
   */
  virtual std::string error(int code) const = 0;
};

}  // namespace cloudstorage

#endif  // IHTTP_H
