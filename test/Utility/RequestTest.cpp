#include "gtest/gtest.h"

#include "Request/Request.h"
#include "Utility/AuthMock.h"
#include "Utility/CloudProviderMock.h"

namespace cloudstorage {

using ::testing::ByMove;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::InvokeArgument;
using ::testing::MockFunction;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;

TEST(RequestTest, ResolvesResult) {
  using ReturnValue = EitherError<std::string>;

  MockFunction<void(std::shared_ptr<Request<ReturnValue>>)> resolver;
  MockFunction<void(ReturnValue)> callback;
  auto request = std::make_shared<Request<ReturnValue>>(
      CloudProviderMock::create(), callback.AsStdFunction(),
      resolver.AsStdFunction());

  EXPECT_CALL(resolver, Call(request))
      .WillOnce(Invoke([](const std::shared_ptr<Request<ReturnValue>>& r) {
        r->done(std::string("test"));
      }));
  EXPECT_CALL(callback,
              Call(Property(&ReturnValue::right, Pointee(Eq("test")))))
      .WillOnce(Return());

  EXPECT_THAT(request->run()->result().right(), Pointee(Eq("test")));
}

TEST(RequestTest, DoesRequest) {
  using ReturnValue = EitherError<std::string>;

  MockFunction<void(std::shared_ptr<Request<ReturnValue>>)> resolver;
  MockFunction<void(ReturnValue)> callback;
  auto provider = CloudProviderMock::create();
  auto request = std::make_shared<Request<ReturnValue>>(
      provider, callback.AsStdFunction(), resolver.AsStdFunction());

  EXPECT_CALL(resolver, Call(request))
      .WillOnce(Invoke([=](const std::shared_ptr<Request<ReturnValue>>& r) {
        r->request(
            [=](const util::Output&) {
              return r->provider()->http()->create("http://example.com",
                                                   "POST");
            },
            [=](const EitherError<Response>&) {
              request->done(std::string("test"));
            });
      }));

  EXPECT_THAT(request->run()->result().right(), Pointee(Eq("test")));
}

TEST(RequestTest, DoesReauthorization) {
  using ReturnValue = EitherError<std::string>;

  MockFunction<void(std::shared_ptr<Request<ReturnValue>>)> resolver;
  MockFunction<void(ReturnValue)> callback;
  auto provider = CloudProviderMock::create();
  auto request = std::make_shared<Request<ReturnValue>>(
      provider, callback.AsStdFunction(), resolver.AsStdFunction());

  EXPECT_CALL(*provider->auth(), refreshTokenRequest).WillOnce(Return([] {
    auto mock_request = std::make_shared<HttpRequestMock>();
    EXPECT_CALL(*mock_request, send)
        .WillOnce(InvokeArgument<0>(IHttpRequest::Response{200}));
    return mock_request;
  }()));
  EXPECT_CALL(*provider->auth(), refreshTokenResponse)
      .WillOnce(Return(ByMove(std::make_unique<IAuth::Token>())));

  int request_count = 0;

  EXPECT_CALL(resolver, Call(request))
      .WillOnce(Invoke(
          [=, &request_count](const std::shared_ptr<Request<ReturnValue>>& r) {
            r->request(
                [=, &request_count](const util::Output&) {
                  auto mock_request = std::make_shared<HttpRequestMock>();
                  EXPECT_CALL(*mock_request, send)
                      .WillOnce(InvokeArgument<0>(IHttpRequest::Response{
                          request_count == 0 ? 401 : 242}));
                  request_count++;
                  return mock_request;
                },
                [=](const EitherError<Response>& e) {
                  ASSERT_NE(e.right(), nullptr);
                  EXPECT_THAT(e.right()->http_code(), Eq(242));
                  request->done(std::string("test"));
                });
          }));

  EXPECT_THAT(request->run()->result().right(), Pointee(Eq("test")));
  EXPECT_EQ(request_count, 2);
}

}  // namespace cloudstorage
