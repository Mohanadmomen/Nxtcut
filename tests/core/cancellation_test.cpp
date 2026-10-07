#include <nxtcut/core/cancellation.hpp>

#include <gtest/gtest.h>

#include <future>
#include <thread>

namespace nxtcut::core {
namespace {

TEST(CancellationTest, DefaultTokenNeverCancelled) {
    const CancellationToken token;
    EXPECT_FALSE(token.is_cancelled());
    EXPECT_TRUE(token.check().has_value());
}

TEST(CancellationTest, CancelPropagatesToAllCopies) {
    CancellationSource source;
    const CancellationToken t1 = source.token();
    const CancellationToken t2 = t1;

    EXPECT_FALSE(t1.is_cancelled());
    EXPECT_FALSE(t2.is_cancelled());
    EXPECT_FALSE(source.is_cancelled());

    source.request_cancel();

    EXPECT_TRUE(t1.is_cancelled());
    EXPECT_TRUE(t2.is_cancelled());
    EXPECT_TRUE(source.is_cancelled());

    const Status s1 = t1.check();
    ASSERT_FALSE(s1.has_value());
    EXPECT_EQ(s1.error().code(), ErrorCode::Cancelled);
    EXPECT_EQ(s1.error().message(), "operation cancelled");
}

TEST(CancellationTest, RequestCancelIsIdempotent) {
    CancellationSource source;
    source.request_cancel();
    source.request_cancel();
    EXPECT_TRUE(source.is_cancelled());
    EXPECT_TRUE(source.token().is_cancelled());
}

TEST(CancellationTest, TokenOutlivesSource) {
    CancellationToken token;
    {
        CancellationSource source;
        token = source.token();
        source.request_cancel();
    }
    EXPECT_TRUE(token.is_cancelled());
    const Status s = token.check();
    ASSERT_FALSE(s.has_value());
    EXPECT_EQ(s.error().code(), ErrorCode::Cancelled);
}

TEST(CancellationTest, CancelFromAnotherThreadObservedSynchronously) {
    CancellationSource source;
    const CancellationToken token = source.token();

    std::promise<void> worker_ready;
    std::promise<void> cancel_observed;

    std::thread worker([token, &worker_ready, &cancel_observed]() {
        worker_ready.set_value();
        while (!token.is_cancelled()) {
            // Spin-poll atomic acquire flag
        }
        cancel_observed.set_value();
    });

    worker_ready.get_future().get();
    source.request_cancel();
    cancel_observed.get_future().get();

    worker.join();
    EXPECT_TRUE(token.is_cancelled());
}

}  // namespace
}  // namespace nxtcut::core
