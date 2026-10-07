#include <nxtcut/core/memory_sink.hpp>

#include <gtest/gtest.h>

#include <thread>
#include <vector>

namespace nxtcut::core {
namespace {

TEST(MemorySinkTest, CapacityClampingAndEvictionOrder) {
    // 0 is clamped to 1
    MemorySink sink_clamped(0);
    EXPECT_EQ(sink_clamped.size(), 0U);

    sink_clamped.write(LogRecord{LogLevel::Info, 1LL, "a", "first"});
    sink_clamped.write(LogRecord{LogLevel::Info, 2LL, "a", "second"});
    EXPECT_EQ(sink_clamped.size(), 1U);
    EXPECT_EQ(sink_clamped.snapshot()[0].message, "second");

    // Standard capacity 3
    MemorySink sink(3);
    sink.write(LogRecord{LogLevel::Info, 1LL, "app", "one"});
    sink.write(LogRecord{LogLevel::Info, 2LL, "app", "two"});
    sink.write(LogRecord{LogLevel::Info, 3LL, "app", "three"});
    EXPECT_EQ(sink.size(), 3U);

    // Evicts oldest ("one")
    sink.write(LogRecord{LogLevel::Info, 4LL, "app", "four"});
    EXPECT_EQ(sink.size(), 3U);
    const auto snap = sink.snapshot();
    EXPECT_EQ(snap[0].message, "two");
    EXPECT_EQ(snap[1].message, "three");
    EXPECT_EQ(snap[2].message, "four");
}

TEST(MemorySinkTest, ClearAndSnapshotCopy) {
    MemorySink sink(5);
    sink.write(LogRecord{LogLevel::Info, 10LL, "app", "entry"});

    auto snap = sink.snapshot();
    EXPECT_EQ(snap.size(), 1U);

    sink.clear();
    EXPECT_EQ(sink.size(), 0U);
    EXPECT_TRUE(sink.snapshot().empty());

    // Original snapshot remains valid and unchanged
    EXPECT_EQ(snap.size(), 1U);
    EXPECT_EQ(snap[0].message, "entry");
}

TEST(MemorySinkTest, ThreadSafety) {
    MemorySink sink(100);
    constexpr int kThreads = 4;
    constexpr int kIterations = 100;

    std::vector<std::thread> threads;
    threads.reserve(kThreads);

    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&sink]() {
            for (int i = 0; i < kIterations; ++i) {
                sink.write(LogRecord{LogLevel::Info, static_cast<std::int64_t>(i), "thread", "data"});
                static_cast<void>(sink.size());
                static_cast<void>(sink.snapshot());
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    EXPECT_LE(sink.size(), 100U);
}

}  // namespace
}  // namespace nxtcut::core
