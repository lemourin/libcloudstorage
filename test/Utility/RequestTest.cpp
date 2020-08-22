#include "gtest/gtest.h"

#include "Request/Request.h"
#include "Utility/AuthMock.h"
#include "Utility/CloudProviderMock.h"

namespace cloudstorage {

using ::testing::Eq;
using ::testing::Invoke;
using ::testing::MockFunction;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;

TEST(RequestTest, ResolvesResult) {
  using ReturnValue = EitherError<std::string>;

  MockFunction<void(std::shared_ptr<Request<ReturnValue>>)> resolver;
  MockFunction<void(ReturnValue)> callback;
  auto request = std::make_shared<Request<EitherError<std::string>>>(
      std::make_shared<CloudProviderMock>(std::make_unique<AuthMock>()),
      callback.AsStdFunction(), resolver.AsStdFunction());

  EXPECT_CALL(resolver, Call(request))
      .WillOnce(Invoke([](const std::shared_ptr<Request<ReturnValue>>& r) {
        r->done(std::string("test"));
      }));
  EXPECT_CALL(callback,
              Call(Property(&ReturnValue::right, Pointee(Eq("test")))))
      .WillOnce(Return());

  EXPECT_THAT(request->run()->result().right(), Pointee(Eq("test")));
}

}  // namespace cloudstorage
