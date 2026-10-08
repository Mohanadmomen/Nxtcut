#include <nxtcut/core/log_format.hpp>

#include <gtest/gtest.h>

namespace nxtcut::core {
namespace {

TEST(LogFormatTest, RequiredVectorArbitraryTimestamp) {
    const LogRecord rec{
        LogLevel::Info,
        1767323045LL * 1000LL + 678LL,
        "engine",
        "hello",
    };
    EXPECT_EQ(format_log_line(rec), "2026-01-02 03:04:05.678 INFO [engine] hello");
}

TEST(LogFormatTest, RequiredVectorEpochZero) {
    const LogRecord rec{
        LogLevel::Warn,
        0LL,
        "a",
        "b",
    };
    EXPECT_EQ(format_log_line(rec), "1970-01-01 00:00:00.000 WARN [a] b");
}

TEST(LogFormatTest, RequiredVectorNegativeTimestamp) {
    const LogRecord rec{
        LogLevel::Error,
        -1LL,
        "a",
        "b",
    };
    EXPECT_EQ(format_log_line(rec), "1969-12-31 23:59:59.999 ERROR [a] b");
}

TEST(LogFormatTest, RequiredVectorLeapYear2000) {
    const LogRecord rec{
        LogLevel::Info,
        951782400LL * 1000LL,
        "test",
        "msg",
    };
    EXPECT_EQ(format_log_line(rec), "2000-02-29 00:00:00.000 INFO [test] msg");
}

TEST(LogFormatTest, RequiredVectorNonLeapYear2100) {
    const LogRecord rec{
        LogLevel::Info,
        4107542400LL * 1000LL,
        "test",
        "msg",
    };
    EXPECT_EQ(format_log_line(rec), "2100-03-01 00:00:00.000 INFO [test] msg");
}

TEST(LogFormatTest, RequiredVectorYear2038) {
    const LogRecord rec{
        LogLevel::Trace,
        2147483648LL * 1000LL,
        "test",
        "msg",
    };
    EXPECT_EQ(format_log_line(rec), "2038-01-19 03:14:08.000 TRACE [test] msg");
}

TEST(LogFormatTest, RequiredVectorBoundaryMilliseconds) {
    const LogRecord rec999{
        LogLevel::Debug,
        999LL,
        "test",
        "msg",
    };
    EXPECT_EQ(format_log_line(rec999), "1970-01-01 00:00:00.999 DEBUG [test] msg");

    const LogRecord rec1000{
        LogLevel::Debug,
        1000LL,
        "test",
        "msg",
    };
    EXPECT_EQ(format_log_line(rec1000), "1970-01-01 00:00:01.000 DEBUG [test] msg");
}

TEST(LogFormatTest, RequiredVectorEmptyLoggerAndEmptyMessage) {
    const LogRecord rec_empty_logger{
        LogLevel::Info,
        0LL,
        "",
        "hello",
    };
    EXPECT_EQ(format_log_line(rec_empty_logger), "1970-01-01 00:00:00.000 INFO [] hello");

    const LogRecord rec_empty_msg{
        LogLevel::Info,
        0LL,
        "engine",
        "",
    };
    // Documented behavior: empty message ends with "] "
    EXPECT_EQ(format_log_line(rec_empty_msg), "1970-01-01 00:00:00.000 INFO [engine] ");
}

}  // namespace
}  // namespace nxtcut::core
