#include <nxtcut/core/log_level.hpp>

#include <gtest/gtest.h>

namespace nxtcut::core {
namespace {

TEST(LogLevelTest, ToString) {
    EXPECT_EQ(to_string(LogLevel::Trace), "TRACE");
    EXPECT_EQ(to_string(LogLevel::Debug), "DEBUG");
    EXPECT_EQ(to_string(LogLevel::Info), "INFO");
    EXPECT_EQ(to_string(LogLevel::Warn), "WARN");
    EXPECT_EQ(to_string(LogLevel::Error), "ERROR");
    EXPECT_EQ(to_string(LogLevel::Critical), "CRITICAL");
    EXPECT_EQ(to_string(LogLevel::Off), "OFF");
}

TEST(LogLevelTest, ParseCaseInsensitive) {
    const auto t1 = parse_log_level("trace");
    ASSERT_TRUE(t1.has_value());
    EXPECT_EQ(*t1, LogLevel::Trace);

    const auto t2 = parse_log_level("TRACE");
    ASSERT_TRUE(t2.has_value());
    EXPECT_EQ(*t2, LogLevel::Trace);

    const auto t3 = parse_log_level("TrAcE");
    ASSERT_TRUE(t3.has_value());
    EXPECT_EQ(*t3, LogLevel::Trace);

    const auto d = parse_log_level("Debug");
    ASSERT_TRUE(d.has_value());
    EXPECT_EQ(*d, LogLevel::Debug);

    const auto i = parse_log_level("INFO");
    ASSERT_TRUE(i.has_value());
    EXPECT_EQ(*i, LogLevel::Info);

    const auto w = parse_log_level("warn");
    ASSERT_TRUE(w.has_value());
    EXPECT_EQ(*w, LogLevel::Warn);

    const auto e = parse_log_level("Error");
    ASSERT_TRUE(e.has_value());
    EXPECT_EQ(*e, LogLevel::Error);

    const auto c = parse_log_level("CRITICAL");
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(*c, LogLevel::Critical);

    const auto o = parse_log_level("off");
    ASSERT_TRUE(o.has_value());
    EXPECT_EQ(*o, LogLevel::Off);
}

TEST(LogLevelTest, ParseInvalid) {
    const auto r1 = parse_log_level("");
    ASSERT_FALSE(r1.has_value());
    EXPECT_EQ(r1.error().code(), ErrorCode::InvalidArgument);

    const auto r2 = parse_log_level("warning");
    ASSERT_FALSE(r2.has_value());
    EXPECT_EQ(r2.error().code(), ErrorCode::InvalidArgument);

    const auto r3 = parse_log_level("information");
    ASSERT_FALSE(r3.has_value());
    EXPECT_EQ(r3.error().code(), ErrorCode::InvalidArgument);

    const auto r4 = parse_log_level("err");
    ASSERT_FALSE(r4.has_value());
    EXPECT_EQ(r4.error().code(), ErrorCode::InvalidArgument);
}

TEST(LogLevelTest, Ordering) {
    EXPECT_LT(LogLevel::Trace, LogLevel::Debug);
    EXPECT_LT(LogLevel::Debug, LogLevel::Info);
    EXPECT_LT(LogLevel::Info, LogLevel::Warn);
    EXPECT_LT(LogLevel::Warn, LogLevel::Error);
    EXPECT_LT(LogLevel::Error, LogLevel::Critical);
    EXPECT_LT(LogLevel::Critical, LogLevel::Off);
}

}  // namespace
}  // namespace nxtcut::core
