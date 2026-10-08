#include <nxtcut/core/logger.hpp>
#include <nxtcut/core/memory_sink.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <thread>
#include <vector>

namespace nxtcut::core {
namespace {

TEST(LoggerTest, LevelFilteringAndOff) {
    auto sink = std::make_shared<MemorySink>(100);
    Logger logger("test", LogLevel::Warn, {sink}, []() { return 1000LL; });

    EXPECT_FALSE(logger.should_log(LogLevel::Trace));
    EXPECT_FALSE(logger.should_log(LogLevel::Debug));
    EXPECT_FALSE(logger.should_log(LogLevel::Info));
    EXPECT_TRUE(logger.should_log(LogLevel::Warn));
    EXPECT_TRUE(logger.should_log(LogLevel::Error));
    EXPECT_TRUE(logger.should_log(LogLevel::Critical));
    EXPECT_FALSE(logger.should_log(LogLevel::Off));

    logger.trace("t");
    logger.debug("d");
    logger.info("i");
    EXPECT_EQ(sink->size(), 0U);

    logger.warn("w");
    logger.error("e");
    logger.critical("c");
    logger.log(LogLevel::Off, "never");
    EXPECT_EQ(sink->size(), 3U);

    logger.set_level(LogLevel::Off);
    EXPECT_EQ(logger.level(), LogLevel::Off);
    EXPECT_FALSE(logger.should_log(LogLevel::Critical));
    logger.critical("another");
    EXPECT_EQ(sink->size(), 3U);
}

TEST(LoggerTest, DeliveryToAllSinksAndNullSinkIgnored) {
    auto sink1 = std::make_shared<MemorySink>(10);
    auto sink2 = std::make_shared<MemorySink>(10);

    // Null sink in vector must be safely ignored
    Logger logger("dual", LogLevel::Debug, {sink1, nullptr, sink2}, []() { return 42LL; });

    logger.info("hello dual");

    EXPECT_EQ(sink1->size(), 1U);
    EXPECT_EQ(sink2->size(), 1U);

    const auto snap1 = sink1->snapshot();
    const auto snap2 = sink2->snapshot();
    ASSERT_EQ(snap1.size(), 1U);
    ASSERT_EQ(snap2.size(), 1U);
    EXPECT_EQ(snap1[0].message, "hello dual");
    EXPECT_EQ(snap2[0].message, "hello dual");
    EXPECT_EQ(snap1[0].unix_millis, 42LL);
    EXPECT_EQ(snap2[0].unix_millis, 42LL);
}

TEST(LoggerTest, ChildNamingAndInheritance) {
    auto sink = std::make_shared<MemorySink>(10);
    Logger parent("engine", LogLevel::Info, {sink}, []() { return 123LL; });

    Logger child = parent.child("render");
    EXPECT_EQ(child.name(), "engine.render");
    EXPECT_EQ(child.level(), LogLevel::Info);

    child.info("frame rendered");
    ASSERT_EQ(sink->size(), 1U);
    EXPECT_EQ(sink->snapshot()[0].logger_name, "engine.render");
}

TEST(LoggerTest, MultiThreadedLogging) {
    constexpr std::size_t kCapacity = 2000;
    auto sink = std::make_shared<MemorySink>(kCapacity);
    Logger logger("concurrent", LogLevel::Trace, {sink}, []() { return 999LL; });

    constexpr int kThreads = 8;
    constexpr int kMsgsPerThread = 200;

    std::vector<std::thread> threads;
    threads.reserve(kThreads);

    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&logger]() {
            for (int i = 0; i < kMsgsPerThread; ++i) {
                logger.info("thread msg");
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    EXPECT_EQ(sink->size(), static_cast<std::size_t>(kThreads * kMsgsPerThread));
}

}  // namespace
}  // namespace nxtcut::core
