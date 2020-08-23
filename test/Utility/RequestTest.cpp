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
using ::testing::SaveArg;

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

TEST(RequestTest, DoesOneReauthorizationForParallelRequests) {
  using ReturnValue = EitherError<std::string>;

  auto provider = CloudProviderMock::create();

  MockFunction<void(std::shared_ptr<Request<ReturnValue>>)> first_resolver;
  MockFunction<void(ReturnValue)> first_callback;
  auto first_request = std::make_shared<Request<ReturnValue>>(
      provider, first_callback.AsStdFunction(), first_resolver.AsStdFunction());

  MockFunction<void(std::shared_ptr<Request<ReturnValue>>)> second_resolver;
  MockFunction<void(ReturnValue)> second_callback;
  auto second_request = std::make_shared<Request<ReturnValue>>(
      provider, second_callback.AsStdFunction(),
      second_resolver.AsStdFunction());

  IHttpRequest::CompleteCallback refresh_token_request_complete;

  EXPECT_CALL(*provider->auth(), refreshTokenRequest)
      .Times(1)
      .WillOnce(Return([&refresh_token_request_complete] {
        auto mock_request = std::make_shared<HttpRequestMock>();
        EXPECT_CALL(*mock_request, send)
            .WillOnce(SaveArg<0>(&refresh_token_request_complete));
        return mock_request;
      }()));
  EXPECT_CALL(*provider->auth(), refreshTokenResponse)
      .WillOnce(Return(ByMove(std::make_unique<IAuth::Token>())));

  int first_request_count = 0;

  EXPECT_CALL(first_resolver, Call(first_request))
      .WillOnce(Invoke([=, &first_request_count](
                           const std::shared_ptr<Request<ReturnValue>>& r) {
        r->request(
            [=, &first_request_count](const util::Output&) {
              auto mock_request = std::make_shared<HttpRequestMock>();
              EXPECT_CALL(*mock_request, send)
                  .WillOnce(InvokeArgument<0>(IHttpRequest::Response{
                      first_request_count == 0 ? 401 : 242}));
              first_request_count++;
              return mock_request;
            },
            [=](const EitherError<Response>& e) {
              ASSERT_NE(e.right(), nullptr);
              EXPECT_THAT(e.right()->http_code(), Eq(242));
              first_request->done(std::string("test1"));
            });
      }));

  int second_request_count = 0;

  EXPECT_CALL(second_resolver, Call(second_request))
      .WillOnce(Invoke([=, &second_request_count](
                           const std::shared_ptr<Request<ReturnValue>>& r) {
        r->request(
            [=, &second_request_count](const util::Output&) {
              auto mock_request = std::make_shared<HttpRequestMock>();
              EXPECT_CALL(*mock_request, send)
                  .WillOnce(InvokeArgument<0>(IHttpRequest::Response{
                      second_request_count == 0 ? 401 : 243}));
              second_request_count++;
              return mock_request;
            },
            [=](const EitherError<Response>& e) {
              ASSERT_NE(e.right(), nullptr);
              EXPECT_THAT(e.right()->http_code(), Eq(243));
              second_request->done(std::string("test2"));
            });
      }));

  auto first_wrapper = first_request->run();
  auto second_wrapper = second_request->run();

  refresh_token_request_complete(IHttpRequest::Response{200});

  EXPECT_THAT(first_wrapper->result().right(), Pointee(Eq("test1")));
  EXPECT_THAT(second_wrapper->result().right(), Pointee(Eq("test2")));
}

TEST(RequestTest, CancelPropagates) {
  using ReturnValue = EitherError<std::string>;

  MockFunction<void(std::shared_ptr<Request<ReturnValue>>)> resolver;
  MockFunction<void(ReturnValue)> callback;
  auto provider = CloudProviderMock::create();
  auto request = std::make_shared<Request<ReturnValue>>(
      provider, callback.AsStdFunction(), resolver.AsStdFunction());

  std::shared_ptr<Request<ReturnValue>> subrequest;
  EXPECT_CALL(resolver, Call(request))
      .WillOnce(Invoke(
          [=, &subrequest](const std::shared_ptr<Request<ReturnValue>>& r) {
            request->make_subrequest<CloudProviderMock>(
                &CloudProviderMock::request<ReturnValue>,
                [provider, &subrequest](
                    const std::function<void(const ReturnValue&)>& callback) {
                  subrequest = std::make_shared<Request<ReturnValue>>(
                      provider, callback,
                      [](const std::shared_ptr<Request<ReturnValue>>& r) {
                        r->done(std::string("test"));
                      });
                  return subrequest->run();
                },
                [=](const ReturnValue& e) { request->done(e.right()); });
          }));

  EXPECT_CALL(callback, Call).WillOnce(Return());

  request->run()->cancel();

  EXPECT_TRUE(request->is_cancelled());
  EXPECT_TRUE(subrequest->is_cancelled());
}

}  // namespace cloudstorage
