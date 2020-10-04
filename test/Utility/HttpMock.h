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

template <typename InputMatcher, typename HttpRequestMatcher>
inline std::shared_ptr<HttpRequestMock> MockResponse(
    int http_code, cloudstorage::IHttpRequest::HeaderParameters headers,
    const char* response, const InputMatcher& input_matcher,
    const HttpRequestMatcher& request_matcher) {
  auto http_response = std::make_shared<HttpRequestMock>();

  EXPECT_CALL(*http_response, send)
      .WillRepeatedly(testing::WithArgs<0, 1, 2, 3>(testing::Invoke(
          [=, http_response = http_response.get(),
           response = std::string(response)](
              const cloudstorage::IHttpRequest::CompleteCallback& on_completed,
              const std::shared_ptr<std::istream>& input_stream,
              const std::shared_ptr<std::ostream>& output_stream,
              const std::shared_ptr<std::ostream>& error_stream) {
            std::stringstream str_stream;
            str_stream << input_stream->rdbuf();

            EXPECT_THAT(*http_response, request_matcher);
            EXPECT_THAT(str_stream.str(), input_matcher);

            if (cloudstorage::IHttpRequest::isSuccess(http_code) &&
                http_code != 301)
              *output_stream << response;
            else
              *error_stream << response;
            on_completed(cloudstorage::IHttpRequest::Response{
                http_code, headers, output_stream, error_stream});
          })));

  return http_response;
}

inline auto IgnoringWhitespace(const std::string& string) {
  return testing::Truly([=](const std::string& input) {
    auto strip_whitespace = [=](const std::string& str) {
      std::string result;
      for (char c : str) {
        if (!std::isspace(c)) {
          result += c;
        }
      }
      return result;
    };
    return strip_whitespace(string) == strip_whitespace(input);
  });
}

class LimitedHttpRequestMatcher;

class HttpResponse {
 public:
  HttpResponse& WithHeaders(
      cloudstorage::IHttpRequest::HeaderParameters headers) {
    headers_ = std::move(headers);
    return *this;
  }

  HttpResponse& WithStatus(int status) {
    status_ = status;
    return *this;
  }

  HttpResponse& WithContent(std::string content) {
    content_ = std::move(content);
    return *this;
  }

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
                     testing::Matcher<const std::string&> url_matcher)
      : http_mock_(http_mock), url_matcher_(std::move(url_matcher)) {}

  ~HttpRequestMatcher() override {
    auto& expectation = EXPECT_CALL(
        http_mock_, create(url_matcher_, method_matcher_, redirect_matcher_));
    for (const auto& r : recorded_requests_) {
      expectation.WillOnce(testing::DoAll(
          testing::WithArgs<0, 1, 2>(testing::Invoke(
              [r = r.get()](const std::string& url, const std::string& method,
                            bool follow_redirect) {
                r->url_ = url;
                r->method_ = method;
                r->follow_redirect_ = follow_redirect;
              })),
          testing::Return(r)));
    }
  }

  template <typename MethodMatcherT>
  HttpRequestMatcher& WithMethod(MethodMatcherT method_matcher) {
    method_matcher_ = std::move(method_matcher);
    return *this;
  }

  HttpRequestMatcher& WithFollowNoRedirect() {
    redirect_matcher_ = false;
    return *this;
  }

  LimitedHttpRequestMatcher& WithBody(StringMatcher body_matcher) override {
    body_matcher_ = std::move(body_matcher);
    return *this;
  }

  LimitedHttpRequestMatcher& WithParameter(
      StringMatcher parameter_matcher, StringMatcher value_matcher) override {
    parameter_matcher_.emplace_back(std::move(parameter_matcher),
                                    std::move(value_matcher));
    return *this;
  }

  LimitedHttpRequestMatcher& WithRequestMatching(
      testing::Matcher<const HttpRequestMock&> matcher) override {
    http_request_mock_matcher_ = std::move(matcher);
    return *this;
  }

  LimitedHttpRequestMatcher& WithHeaderParameter(
      StringMatcher parameter_matcher, StringMatcher value_matcher) override {
    header_parameter_matcher_.emplace_back(std::move(parameter_matcher),
                                           std::move(value_matcher));
    return *this;
  }

  LimitedHttpRequestMatcher& WillRespondWith(const char* string) override {
    recorded_requests_.emplace_back(MockResponse(200, {}, string, body_matcher_,
                                                 http_request_mock_matcher_));
    SetUpExpectations(recorded_requests_.back());
    return *this;
  }

  LimitedHttpRequestMatcher& WillRespondWithCode(int code) override {
    recorded_requests_.emplace_back(
        MockResponse(code, {}, "", body_matcher_, http_request_mock_matcher_));
    SetUpExpectations(recorded_requests_.back());
    return *this;
  }

  LimitedHttpRequestMatcher& WillRespondWith(HttpResponse response) override {
    recorded_requests_.emplace_back(MockResponse(
        response.status_, response.headers_, response.content_.c_str(),
        body_matcher_, http_request_mock_matcher_));
    SetUpExpectations(recorded_requests_.back());
    return *this;
  }

  LimitedHttpRequestMatcher& AndThen() override {
    parameter_matcher_.clear();
    header_parameter_matcher_.clear();
    body_matcher_ = testing::_;
    http_request_mock_matcher_ = testing::_;
    return *this;
  }

 private:
  void SetUpExpectations(const std::shared_ptr<HttpRequestMock>& request) {
    if (parameter_matcher_.empty()) {
      EXPECT_CALL(*request, setParameter(testing::_, testing::_))
          .WillRepeatedly(testing::WithArgs<0, 1>(testing::Invoke(
              [r = request.get()](std::string value, std::string param) {
                r->get_parameters_.emplace(std::move(value), std::move(param));
              })));
    }
    if (header_parameter_matcher_.empty()) {
      EXPECT_CALL(*request, setHeaderParameter(testing::_, testing::_))
          .WillRepeatedly(testing::WithArgs<0, 1>(testing::Invoke(
              [r = request.get()](std::string value, std::string param) {
                r->header_parameters_.emplace(std::move(value),
                                              std::move(param));
              })));
    }
    for (const auto& parameter_matcher : parameter_matcher_) {
      EXPECT_CALL(*request, setParameter(parameter_matcher.first,
                                         parameter_matcher.second))
          .WillOnce(testing::WithArgs<0, 1>(testing::Invoke(
              [r = request.get()](std::string value, std::string param) {
                r->get_parameters_.emplace(std::move(value), std::move(param));
              })));
    }
    for (const auto& header_parameter_matcher : header_parameter_matcher_) {
      EXPECT_CALL(*request, setHeaderParameter(header_parameter_matcher.first,
                                               header_parameter_matcher.second))
          .WillOnce(testing::WithArgs<0, 1>(testing::Invoke(
              [r = request.get()](std::string value, std::string param) {
                r->header_parameters_.emplace(std::move(value),
                                              std::move(param));
              })));
    }
  }

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

template <typename UrlMatcherT>
inline HttpRequestMatcher ExpectHttp(HttpMock* http_mock,
                                     const UrlMatcherT& url_matcher) {
  return HttpRequestMatcher(*http_mock, url_matcher);
}

#endif  // HTTP_MOCK_H
