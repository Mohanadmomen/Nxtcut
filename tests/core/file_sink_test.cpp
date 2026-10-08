#include <nxtcut/core/file_sink.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace nxtcut::core {
namespace {

struct TempDirectoryFixture {
    std::filesystem::path path;

    TempDirectoryFixture() {
        const auto now_ns = std::chrono::steady_clock::now().time_since_epoch().count();
        path =
            std::filesystem::temp_directory_path() / ("nxtcut_fs_test_" + std::to_string(now_ns));
        std::filesystem::create_directories(path);
    }

    ~TempDirectoryFixture() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
};

std::string read_file_string(const std::filesystem::path& file_path) {
    std::ifstream in(file_path, std::ios::in | std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return content;
}

TEST(FileSinkTest, UnwritablePathReturnsIoError) {
    TempDirectoryFixture fixture;
    const auto bad_path = fixture.path / "missing_dir" / "nested" / "test.log";

    auto sink_res = FileSink::create(bad_path, FileSink::Mode::Truncate);
    ASSERT_FALSE(sink_res.has_value());
    EXPECT_EQ(sink_res.error().code(), ErrorCode::IoError);
}

TEST(FileSinkTest, AppendVsTruncate) {
    TempDirectoryFixture fixture;
    const auto log_file = fixture.path / "test.log";

    // 1. Initial write
    {
        auto sink_res = FileSink::create(log_file, FileSink::Mode::Truncate);
        ASSERT_TRUE(sink_res.has_value());
        auto sink = std::move(*sink_res);
        sink->write(LogRecord{LogLevel::Info, 0LL, "app", "line 1"});
    }
    EXPECT_EQ(read_file_string(log_file), "1970-01-01 00:00:00.000 INFO [app] line 1\n");

    // 2. Truncate mode overwrites
    {
        auto sink_res = FileSink::create(log_file, FileSink::Mode::Truncate);
        ASSERT_TRUE(sink_res.has_value());
        auto sink = std::move(*sink_res);
        sink->write(LogRecord{LogLevel::Info, 0LL, "app", "line 2"});
    }
    EXPECT_EQ(read_file_string(log_file), "1970-01-01 00:00:00.000 INFO [app] line 2\n");

    // 3. Append mode appends
    {
        auto sink_res = FileSink::create(log_file, FileSink::Mode::Append);
        ASSERT_TRUE(sink_res.has_value());
        auto sink = std::move(*sink_res);
        sink->write(LogRecord{LogLevel::Warn, 0LL, "app", "line 3"});
    }
    EXPECT_EQ(read_file_string(log_file),
              "1970-01-01 00:00:00.000 INFO [app] line 2\n"
              "1970-01-01 00:00:00.000 WARN [app] line 3\n");
}

}  // namespace
}  // namespace nxtcut::core
