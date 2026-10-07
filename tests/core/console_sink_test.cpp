#include <nxtcut/core/console_sink.hpp>

#include <gtest/gtest.h>

#include <sstream>

namespace nxtcut::core {
namespace {

TEST(ConsoleSinkTest, WritesToOStream) {
    std::ostringstream oss;
    ConsoleSink sink(oss);

    const LogRecord record{
        LogLevel::Info,
        0LL,
        "test",
        "hello stream",
    };

    sink.write(record);
    sink.flush();

    const std::string output = oss.str();
    EXPECT_EQ(output, "1970-01-01 00:00:00.000 INFO [test] hello stream\n");
}

}  // namespace
}  // namespace nxtcut::core
