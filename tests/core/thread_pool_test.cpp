#include <nxtcut/core/thread_pool.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <future>
#include <mutex>
#include <stdexcept>
#include <vector>

namespace nxtcut::core {
namespace {

TEST(ThreadPoolTest, CreateZeroThreadsFails) {
    const auto res = ThreadPool::create(0);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), ErrorCode::InvalidArgument);
}

TEST(ThreadPoolTest, DefaultThreadCountPositive) {
    EXPECT_GE(ThreadPool::default_thread_count(), 1U);
}

TEST(ThreadPoolTest, ResultsThroughFutures) {
    auto pool_res = ThreadPool::create(2);
    ASSERT_TRUE(pool_res.has_value());
    auto pool = std::move(*pool_res);

    auto fut = pool->submit([]() { return 42; });
    ASSERT_TRUE(fut.has_value());
    EXPECT_EQ(fut->get(), 42);
}

TEST(ThreadPoolTest, PriorityOrderAndFifoWithinPriority) {
    auto pool_res = ThreadPool::create(1);
    ASSERT_TRUE(pool_res.has_value());
    auto pool = std::move(*pool_res);

    std::promise<void> gate_started;
    std::promise<void> gate_release;
    auto release_fut = gate_release.get_future().share();

    // High gate task occupies the single worker
    auto gate_task_fut = pool->submit([&gate_started, release_fut]() {
        gate_started.set_value();
        release_fut.get();
    }, TaskPriority::High);
    ASSERT_TRUE(gate_task_fut.has_value());

    // Wait until worker is actively holding the gate
    gate_started.get_future().get();

    std::mutex order_mutex;
    std::vector<int> recorded_ids;
    auto record = [&](int id) {
        std::lock_guard<std::mutex> lock(order_mutex);
        recorded_ids.push_back(id);
    };

    // ONLY after started is received, enqueue Low(1), Normal(2), High(3), Normal(4)
    ASSERT_TRUE(pool->submit([&]() { record(1); }, TaskPriority::Low).has_value());
    ASSERT_TRUE(pool->submit([&]() { record(2); }, TaskPriority::Normal).has_value());
    ASSERT_TRUE(pool->submit([&]() { record(3); }, TaskPriority::High).has_value());
    ASSERT_TRUE(pool->submit([&]() { record(4); }, TaskPriority::Normal).has_value());

    // Release worker
    gate_release.set_value();
    pool->wait_idle();

    // High(3) must run first, then Normal(2), Normal(4) [FIFO within Normal], then Low(1)
    const std::vector<int> expected{3, 2, 4, 1};
    EXPECT_EQ(recorded_ids, expected);
}

TEST(ThreadPoolTest, SubmitAfterShutdownReturnsCancelled) {
    auto pool_res = ThreadPool::create(2);
    ASSERT_TRUE(pool_res.has_value());
    auto pool = std::move(*pool_res);

    pool->shutdown();

    auto submit_res = pool->submit([]() {});
    ASSERT_FALSE(submit_res.has_value());
    EXPECT_EQ(submit_res.error().code(), ErrorCode::Cancelled);
    EXPECT_EQ(submit_res.error().message(), "thread pool is shut down");
}

TEST(ThreadPoolTest, ShutdownRunsAllQueuedTasks) {
    auto pool_res = ThreadPool::create(1);
    ASSERT_TRUE(pool_res.has_value());
    auto pool = std::move(*pool_res);

    std::promise<void> gate_started;
    std::promise<void> gate_release;
    auto release_fut = gate_release.get_future().share();

    auto gate_fut = pool->submit([&gate_started, release_fut]() {
        gate_started.set_value();
        release_fut.get();
    });
    ASSERT_TRUE(gate_fut.has_value());

    gate_started.get_future().get();

    std::atomic<int> completed{0};
    for (int i = 0; i < 5; ++i) {
        ASSERT_TRUE(pool->submit([&completed]() { ++completed; }).has_value());
    }

    gate_release.set_value();
    pool->shutdown();

    EXPECT_EQ(completed.load(), 5);
}

TEST(ThreadPoolTest, WaitIdleBlocksUntilEmpty) {
    auto pool_res = ThreadPool::create(2);
    ASSERT_TRUE(pool_res.has_value());
    auto pool = std::move(*pool_res);

    std::atomic<int> counter{0};
    for (int i = 0; i < 50; ++i) {
        ASSERT_TRUE(pool->submit([&counter]() { ++counter; }).has_value());
    }

    pool->wait_idle();
    EXPECT_EQ(counter.load(), 50);
}

TEST(ThreadPoolTest, TaskExceptionCapturedByFuture) {
    auto pool_res = ThreadPool::create(1);
    ASSERT_TRUE(pool_res.has_value());
    auto pool = std::move(*pool_res);

    auto fut = pool->submit([]() -> int {
        throw std::runtime_error("simulated task failure");
    });
    ASSERT_TRUE(fut.has_value());

    EXPECT_THROW(fut->get(), std::runtime_error);

    // Pool remains operational after task exception
    auto next_fut = pool->submit([]() { return 100; });
    ASSERT_TRUE(next_fut.has_value());
    EXPECT_EQ(next_fut->get(), 100);
}

TEST(ThreadPoolTest, DestructorRunsQueuedTasks) {
    std::atomic<int> finished{0};
    {
        auto pool_res = ThreadPool::create(1);
        ASSERT_TRUE(pool_res.has_value());
        auto pool = std::move(*pool_res);

        std::promise<void> gate_started;
        std::promise<void> gate_release;
        auto release_fut = gate_release.get_future().share();

        ASSERT_TRUE(pool->submit([&gate_started, release_fut]() {
            gate_started.set_value();
            release_fut.get();
        }).has_value());

        gate_started.get_future().get();

        for (int i = 0; i < 5; ++i) {
            ASSERT_TRUE(pool->submit([&finished]() { ++finished; }).has_value());
        }

        gate_release.set_value();
        // Destruction happens here and must invoke shutdown()
    }
    EXPECT_EQ(finished.load(), 5);
}

TEST(ThreadPoolTest, HighConcurrencyMultipleWorkers) {
    auto pool_res = ThreadPool::create(4);
    ASSERT_TRUE(pool_res.has_value());
    auto pool = std::move(*pool_res);

    std::atomic<int> counter{0};
    constexpr int kTaskCount = 1000;

    for (int i = 0; i < kTaskCount; ++i) {
        ASSERT_TRUE(pool->submit([&counter]() {
            counter.fetch_add(1, std::memory_order_relaxed);
        }).has_value());
    }

    pool->wait_idle();
    EXPECT_EQ(counter.load(), kTaskCount);
}

TEST(ThreadPoolTest, ConcurrentShutdownCallsAreSafe) {
    auto pool_res = ThreadPool::create(4);
    ASSERT_TRUE(pool_res.has_value());
    auto pool = std::move(*pool_res);

    std::atomic<int> counter{0};
    constexpr int kTaskCount = 200;
    for (int i = 0; i < kTaskCount; ++i) {
        ASSERT_TRUE(pool->submit([&counter]() {
            counter.fetch_add(1, std::memory_order_relaxed);
        }).has_value());
    }

    std::promise<void> gate_promise;
    std::shared_future<void> gate_future = gate_promise.get_future().share();

    std::vector<std::thread> shutdown_threads;
    shutdown_threads.reserve(4);
    for (int i = 0; i < 4; ++i) {
        shutdown_threads.emplace_back([&pool, gate_future]() {
            gate_future.get();
            pool->shutdown();
        });
    }

    gate_promise.set_value();

    for (auto& th : shutdown_threads) {
        th.join();
    }

    EXPECT_EQ(counter.load(), kTaskCount);

    auto submit_after = pool->submit([]() {});
    ASSERT_FALSE(submit_after.has_value());
    EXPECT_EQ(submit_after.error().code(), ErrorCode::Cancelled);
}

TEST(ThreadPoolTest, TaskCanSubmitTaskWhilePoolIsAlive) {
    auto pool_res = ThreadPool::create(2);
    ASSERT_TRUE(pool_res.has_value());
    auto pool = std::move(*pool_res);

    std::atomic<bool> flag{false};
    ThreadPool* const pool_ptr = pool.get();

    auto first_task_res = pool->submit([pool_ptr, &flag]() {
        auto second_task_res = pool_ptr->submit([&flag]() {
            flag.store(true, std::memory_order_relaxed);
        });
        EXPECT_TRUE(second_task_res.has_value());
    }, TaskPriority::Normal);
    ASSERT_TRUE(first_task_res.has_value());

    pool->wait_idle();

    EXPECT_TRUE(flag.load());
}

}  // namespace
}  // namespace nxtcut::core
