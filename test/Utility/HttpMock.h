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

  MOCK_CONST_METHOD0(parameters, const GetParameters&());

  MOCK_CONST_METHOD0(headerParameters, const HeaderParameters&());

  MOCK_CONST_METHOD0(url, const std::string&());

  MOCK_CONST_METHOD0(method, const std::string&());

  MOCK_CONST_METHOD0(follow_redirect, bool());

  MOCK_CONST_METHOD5(send, void(CompleteCallback on_completed,
                                std::shared_ptr<std::istream> data,
                                std::shared_ptr<std::ostream> response,
                                std::shared_ptr<std::ostream> error_stream,
                                ICallback::Pointer callback));

  std::string url_ = "http://example.com";
  std::string method_ = "GET";
};

class HttpMock : public cloudstorage::IHttp {
 public:
  MOCK_CONST_METHOD3(
      create, cloudstorage::IHttpRequest::Pointer(const std::string& url,
                                                  const std::string& method,
                                                  bool follow_redirect));
};

template <typename InputMatcher>
inline std::shared_ptr<HttpRequestMock> MockResponse(
    int http_code, cloudstorage::IHttpRequest::HeaderParameters headers,
    const char* response, const InputMatcher& input_matcher) {
  auto http_response = std::make_shared<HttpRequestMock>();
  EXPECT_CALL(*http_response, url)
      .WillRepeatedly(testing::ReturnRef(http_response->url_));
  EXPECT_CALL(*http_response, method)
      .WillRepeatedly(testing::ReturnRef(http_response->method_));

  EXPECT_CALL(*http_response, send)
      .WillRepeatedly(testing::WithArgs<0, 1, 2, 3>(testing::Invoke(
          [=](const cloudstorage::IHttpRequest::CompleteCallback& on_completed,
              const std::shared_ptr<std::istream>& input_stream,
              const std::shared_ptr<std::ostream>& output_stream,
              const std::shared_ptr<std::ostream>& error_stream) {
            std::stringstream str_stream;
            str_stream << input_stream->rdbuf();

            EXPECT_THAT(str_stream.str(), input_matcher);

            if (cloudstorage::IHttpRequest::isSuccess(http_code))
              *output_stream << response;
            else
              *error_stream << response;
            on_completed(cloudstorage::IHttpRequest::Response{
                http_code, headers, output_stream, error_stream});
          })));

  return http_response;
}

template <typename InputMatcher>
inline std::shared_ptr<HttpRequestMock> MockResponse(
    const char* response, const InputMatcher& input_matcher) {
  return MockResponse(200, {}, response, input_matcher);
}

inline std::shared_ptr<HttpRequestMock> MockResponse(
    int http_code, const char* response = "") {
  return MockResponse(http_code, {}, response,
                      testing::Truly([](const std::string&) { return true; }));
}

inline std::shared_ptr<HttpRequestMock> MockResponse(const char* response) {
  return MockResponse(200, response);
}

template <typename InputMatcher>
inline std::shared_ptr<HttpRequestMock> MockResponse(
    int http_code, const char* response, const InputMatcher& input_matcher) {
  return MockResponse(http_code, {}, response, input_matcher);
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

class HttpResponseMatcher {
 public:
  virtual HttpResponseMatcher& WillRespondWith(const char* string) = 0;
  virtual HttpResponseMatcher& WillRespondWithCode(int code) = 0;
};

class HttpRequestMatcher : public HttpResponseMatcher {
 public:
  HttpRequestMatcher(HttpMock& http_mock,
                     testing::Matcher<const std::string&> url_matcher)
      : http_mock_(http_mock), url_matcher_(std::move(url_matcher)) {}

  template <typename MethodMatcherT>
  HttpRequestMatcher& WithMethod(MethodMatcherT method_matcher) {
    method_matcher_ = std::move(method_matcher);
    return *this;
  }

  template <typename BodyMatcherT>
  HttpRequestMatcher& WithBody(BodyMatcherT body_matcher) {
    body_matcher_ = std::move(body_matcher);
    return *this;
  }

  template <typename GetParameterMatcherT, typename GetParameterValueMatcherT>
  HttpRequestMatcher& WithParameter(GetParameterMatcherT parameter_matcher,
                                    GetParameterValueMatcherT value_matcher) {
    parameter_matcher_.emplace_back(std::move(parameter_matcher),
                                    std::move(value_matcher));
    return *this;
  }
  template <typename HeaderParameterMatcherT,
            typename HeaderParameterValueMatcherT>
  HttpRequestMatcher& WithHeaderParameter(
      HeaderParameterMatcherT parameter_matcher,
      HeaderParameterValueMatcherT value_matcher) {
    header_parameter_matcher_.emplace_back(std::move(parameter_matcher),
                                           std::move(value_matcher));
    return *this;
  }

  HttpResponseMatcher& WillRespondWith(const char* string) override {
    SetUpExpectations(MockResponse(string, body_matcher_));
    return *this;
  }

  HttpRequestMatcher& WillRespondWithCode(int code) override {
    SetUpExpectations(MockResponse(code));
    return *this;
  }

 private:
  void SetUpExpectations(const std::shared_ptr<HttpRequestMock>& request) {
    for (const auto& parameter_matcher : parameter_matcher_) {
      EXPECT_CALL(*request, setParameter(parameter_matcher.first,
                                         parameter_matcher.second));
    }
    for (const auto& header_parameter_matcher : header_parameter_matcher_) {
      EXPECT_CALL(*request,
                  setHeaderParameter(header_parameter_matcher.first,
                                     header_parameter_matcher.second));
    }
    EXPECT_CALL(http_mock_,
                create(url_matcher_, method_matcher_, redirect_matcher_))
        .WillOnce(testing::Return(request));
  }

  HttpMock& http_mock_;
  testing::Matcher<const std::string&> url_matcher_;
  testing::Matcher<const std::string&> method_matcher_ = testing::_;
  testing::Matcher<bool> redirect_matcher_ = testing::_;
  std::vector<std::pair<testing::Matcher<const std::string&>,
                        testing::Matcher<const std::string&>>>
      parameter_matcher_;
  std::vector<std::pair<testing::Matcher<const std::string&>,
                        testing::Matcher<const std::string&>>>
      header_parameter_matcher_;
  testing::Matcher<std::string> body_matcher_ = testing::_;
};

template <typename UrlMatcherT>
inline HttpRequestMatcher ExpectHttp(HttpMock* http_mock,
                                     const UrlMatcherT& url_matcher) {
  return HttpRequestMatcher(*http_mock, url_matcher);
}

#endif  // HTTP_MOCK_H
