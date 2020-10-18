#include "HttpMock.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::Truly;
using ::testing::WithArgs;

namespace {
template <typename InputMatcher, typename HttpRequestMatcher>
std::shared_ptr<HttpRequestMock> MockResponse(
    int http_code, cloudstorage::IHttpRequest::HeaderParameters headers,
    const char* response, const InputMatcher& input_matcher,
    const HttpRequestMatcher& request_matcher) {
  auto http_response = std::make_shared<HttpRequestMock>();

  EXPECT_CALL(*http_response, send)
      .WillRepeatedly(WithArgs<0, 1, 2, 3>(Invoke(
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
}  // namespace

HttpResponse& HttpResponse::WithHeaders(
    cloudstorage::IHttpRequest::HeaderParameters headers) {
  headers_ = std::move(headers);
  return *this;
}

HttpResponse& HttpResponse::WithStatus(int status) {
  status_ = status;
  return *this;
}

HttpResponse& HttpResponse::WithContent(std::string content) {
  content_ = std::move(content);
  return *this;
}

HttpRequestMatcher::HttpRequestMatcher(
    HttpMock& http_mock, testing::Matcher<const std::string&> url_matcher)
    : http_mock_(http_mock), url_matcher_(std::move(url_matcher)) {}

HttpRequestMatcher::~HttpRequestMatcher() {
  auto& expectation = EXPECT_CALL(
      http_mock_, create(url_matcher_, method_matcher_, redirect_matcher_));
  for (const auto& r : recorded_requests_) {
    expectation.WillOnce(
        DoAll(WithArgs<0, 1, 2>(Invoke([r = r.get()](const std::string& url,
                                                     const std::string& method,
                                                     bool follow_redirect) {
                r->url_ = url;
                r->method_ = method;
                r->follow_redirect_ = follow_redirect;
              })),
              Return(r)));
  }
}

HttpRequestMatcher& HttpRequestMatcher::WithMethod(
    StringMatcher method_matcher) {
  method_matcher_ = std::move(method_matcher);
  return *this;
}

HttpRequestMatcher& HttpRequestMatcher::WithFollowNoRedirect() {
  redirect_matcher_ = false;
  return *this;
}

LimitedHttpRequestMatcher& HttpRequestMatcher::WithBody(
    StringMatcher body_matcher) {
  body_matcher_ = std::move(body_matcher);
  return *this;
}

LimitedHttpRequestMatcher& HttpRequestMatcher::WithParameter(
    StringMatcher parameter_matcher, StringMatcher value_matcher) {
  parameter_matcher_.emplace_back(std::move(parameter_matcher),
                                  std::move(value_matcher));
  return *this;
}

LimitedHttpRequestMatcher& HttpRequestMatcher::WithRequestMatching(
    testing::Matcher<const HttpRequestMock&> matcher) {
  http_request_mock_matcher_ = std::move(matcher);
  return *this;
}

LimitedHttpRequestMatcher& HttpRequestMatcher::WithHeaderParameter(
    StringMatcher parameter_matcher, StringMatcher value_matcher) {
  header_parameter_matcher_.emplace_back(std::move(parameter_matcher),
                                         std::move(value_matcher));
  return *this;
}

LimitedHttpRequestMatcher& HttpRequestMatcher::WillRespondWith(
    const char* string) {
  recorded_requests_.emplace_back(
      MockResponse(200, {}, string, body_matcher_, http_request_mock_matcher_));
  SetUpExpectations(recorded_requests_.back());
  return *this;
}

LimitedHttpRequestMatcher& HttpRequestMatcher::WillRespondWithCode(int code) {
  recorded_requests_.emplace_back(
      MockResponse(code, {}, "", body_matcher_, http_request_mock_matcher_));
  SetUpExpectations(recorded_requests_.back());
  return *this;
}

LimitedHttpRequestMatcher& HttpRequestMatcher::WillRespondWith(
    HttpResponse response) {
  recorded_requests_.emplace_back(MockResponse(
      response.status_, response.headers_, response.content_.c_str(),
      body_matcher_, http_request_mock_matcher_));
  SetUpExpectations(recorded_requests_.back());
  return *this;
}

LimitedHttpRequestMatcher& HttpRequestMatcher::AndThen() {
  parameter_matcher_.clear();
  header_parameter_matcher_.clear();
  body_matcher_ = _;
  http_request_mock_matcher_ = _;
  return *this;
}

void HttpRequestMatcher::SetUpExpectations(
    const std::shared_ptr<HttpRequestMock>& request) {
  if (parameter_matcher_.empty()) {
    EXPECT_CALL(*request, setParameter(_, _))
        .WillRepeatedly(WithArgs<0, 1>(
            Invoke([r = request.get()](std::string value, std::string param) {
              r->get_parameters_.emplace(std::move(value), std::move(param));
            })));
  }
  if (header_parameter_matcher_.empty()) {
    EXPECT_CALL(*request, setHeaderParameter(_, _))
        .WillRepeatedly(WithArgs<0, 1>(
            Invoke([r = request.get()](std::string value, std::string param) {
              r->header_parameters_.emplace(std::move(value), std::move(param));
            })));
  }
  for (const auto& parameter_matcher : parameter_matcher_) {
    EXPECT_CALL(*request,
                setParameter(parameter_matcher.first, parameter_matcher.second))
        .WillOnce(WithArgs<0, 1>(
            Invoke([r = request.get()](std::string value, std::string param) {
              r->get_parameters_.emplace(std::move(value), std::move(param));
            })));
  }
  for (const auto& header_parameter_matcher : header_parameter_matcher_) {
    EXPECT_CALL(*request, setHeaderParameter(header_parameter_matcher.first,
                                             header_parameter_matcher.second))
        .WillOnce(WithArgs<0, 1>(
            Invoke([r = request.get()](std::string value, std::string param) {
              r->header_parameters_.emplace(std::move(value), std::move(param));
            })));
  }
}

testing::Matcher<const std::string&> IgnoringWhitespace(
    const std::string& string) {
  return Truly([=](const std::string& input) {
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
