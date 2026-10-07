#include <nxtcut/core/unique_task.hpp>

#include <gtest/gtest.h>

#include <future>
#include <memory>

namespace nxtcut::core {
namespace {

TEST(UniqueTaskTest, EmptyTaskIsNoOp) {
    UniqueTask task;
    EXPECT_FALSE(static_cast<bool>(task));
    // Calling empty task is a documented safe no-op
    task();
    EXPECT_FALSE(static_cast<bool>(task));
}

TEST(UniqueTaskTest, HoldsMoveOnlyLambda) {
    auto uptr = std::make_unique<int>(42);
    bool executed = false;

    UniqueTask task([p = std::move(uptr), &executed]() {
        executed = true;
        EXPECT_EQ(*p, 42);
    });

    EXPECT_TRUE(static_cast<bool>(task));
    task();
    EXPECT_TRUE(executed);
}

TEST(UniqueTaskTest, MoveLeavesSourceEmpty) {
    bool executed = false;
    UniqueTask task1([&executed]() { executed = true; });

    EXPECT_TRUE(static_cast<bool>(task1));
    UniqueTask task2 = std::move(task1);

    EXPECT_FALSE(static_cast<bool>(task1));
    EXPECT_TRUE(static_cast<bool>(task2));

    task1();  // Must safely do nothing
    EXPECT_FALSE(executed);

    task2();
    EXPECT_TRUE(executed);
}

TEST(UniqueTaskTest, HoldsPackagedTask) {
    std::packaged_task<int()> pt([]() { return 777; });
    auto fut = pt.get_future();

    UniqueTask task(std::move(pt));
    EXPECT_TRUE(static_cast<bool>(task));

    task();
    ASSERT_TRUE(fut.valid());
    EXPECT_EQ(fut.get(), 777);
}

}  // namespace
}  // namespace nxtcut::core
