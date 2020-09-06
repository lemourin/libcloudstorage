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

inline std::shared_ptr<HttpRequestMock> MockResponse(int http_code,
                                                     const char* response) {
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

#endif  // HTTP_MOCK_H
