/*****************************************************************************
 * HttpMock.h
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
#ifndef HTTP_MOCK_H
#define HTTP_MOCK_H

#include "IHttp.h"
#include "gmock/gmock.h"

class LimitedHttpRequestMatcher;

class HttpRequestMock : public cloudstorage::IHttpRequest {
 public:
  MOCK_METHOD2(setParameter,
               void(const std::string& parameter, const std::string& value));

  MOCK_METHOD2(setHeaderParameter,
               void(const std::string& parameter, const std::string& value));

  const GetParameters& parameters() const override { return get_parameters_; }

  const HeaderParameters& headerParameters() const override {
    return header_parameters_;
  }

  const std::string& url() const override { return url_; }

  const std::string& method() const override { return method_; }

  bool follow_redirect() const override { return follow_redirect_; }

  MOCK_CONST_METHOD5(send, void(CompleteCallback on_completed,
                                std::shared_ptr<std::istream> data,
                                std::shared_ptr<std::ostream> response,
                                std::shared_ptr<std::ostream> error_stream,
                                ICallback::Pointer callback));

 private:
  friend class HttpRequestMatcher;

  std::string url_;
  std::string method_;
  bool follow_redirect_ = true;
  GetParameters get_parameters_;
  HeaderParameters header_parameters_;
};

class HttpMock : public cloudstorage::IHttp {
 public:
  MOCK_CONST_METHOD3(
      create, cloudstorage::IHttpRequest::Pointer(const std::string& url,
                                                  const std::string& method,
                                                  bool follow_redirect));
};

class HttpResponse {
 public:
  HttpResponse& WithHeaders(
      cloudstorage::IHttpRequest::HeaderParameters headers);

  HttpResponse& WithStatus(int status);
  HttpResponse& WithContent(std::string content);

 private:
  friend class HttpRequestMatcher;

  int status_ = 200;
  cloudstorage::IHttpRequest::HeaderParameters headers_;
  std::string content_;
};

class HttpResponseMatcher {
 public:
  virtual ~HttpResponseMatcher() = default;

  virtual HttpResponseMatcher& WillRespondWith(const char* string) = 0;
  virtual HttpResponseMatcher& WillRespondWith(HttpResponse) = 0;
  virtual HttpResponseMatcher& WillRespondWithCode(int code) = 0;
  virtual LimitedHttpRequestMatcher& AndThen() = 0;
};

class LimitedHttpRequestMatcher : public HttpResponseMatcher {
 public:
  using StringMatcher = testing::Matcher<const std::string&>;

  virtual LimitedHttpRequestMatcher& WithBody(StringMatcher) = 0;
  virtual LimitedHttpRequestMatcher& WithParameter(StringMatcher,
                                                   StringMatcher) = 0;
  virtual LimitedHttpRequestMatcher& WithHeaderParameter(StringMatcher,
                                                         StringMatcher) = 0;
  virtual LimitedHttpRequestMatcher& WithRequestMatching(
      testing::Matcher<const HttpRequestMock&> matcher) = 0;
};

class HttpRequestMatcher : public LimitedHttpRequestMatcher {
 public:
  HttpRequestMatcher(HttpMock& http_mock,
                     testing::Matcher<const std::string&> url_matcher);
  ~HttpRequestMatcher() override;

  HttpRequestMatcher& WithMethod(StringMatcher method_matcher);

  HttpRequestMatcher& WithFollowNoRedirect();

  LimitedHttpRequestMatcher& WithBody(StringMatcher body_matcher) override;

  LimitedHttpRequestMatcher& WithParameter(
      StringMatcher parameter_matcher, StringMatcher value_matcher) override;

  LimitedHttpRequestMatcher& WithRequestMatching(
      testing::Matcher<const HttpRequestMock&> matcher) override;

  LimitedHttpRequestMatcher& WithHeaderParameter(
      StringMatcher parameter_matcher, StringMatcher value_matcher) override;

  LimitedHttpRequestMatcher& WillRespondWith(const char* string) override;

  LimitedHttpRequestMatcher& WillRespondWithCode(int code) override;

  LimitedHttpRequestMatcher& WillRespondWith(HttpResponse response) override;

  LimitedHttpRequestMatcher& AndThen() override;

 private:
  void SetUpExpectations(const std::shared_ptr<HttpRequestMock>& request);

  HttpMock& http_mock_;
  StringMatcher url_matcher_;
  StringMatcher method_matcher_ = "GET";
  testing::Matcher<bool> redirect_matcher_ = true;
  testing::Matcher<const HttpRequestMock&> http_request_mock_matcher_ =
      testing::_;
  std::vector<std::pair<StringMatcher, StringMatcher>> parameter_matcher_;
  std::vector<std::pair<StringMatcher, StringMatcher>>
      header_parameter_matcher_;
  StringMatcher body_matcher_ = testing::_;
  std::vector<std::shared_ptr<HttpRequestMock>> recorded_requests_;
};

testing::Matcher<const std::string&> IgnoringWhitespace(
    const std::string& string);

template <typename UrlMatcherT>
HttpRequestMatcher ExpectHttp(HttpMock* http_mock,
                              const UrlMatcherT& url_matcher) {
  return HttpRequestMatcher(*http_mock, url_matcher);
}

#endif  // HTTP_MOCK_H
