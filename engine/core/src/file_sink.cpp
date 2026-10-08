#include <nxtcut/core/file_sink.hpp>
#include <nxtcut/core/log_format.hpp>

#include <memory>
#include <string>
#include <utility>

namespace nxtcut::core {

FileSink::FileSink(ConstructToken, std::ofstream stream) : stream_(std::move(stream)) {}

FileSink::~FileSink() {
    flush();
    if (stream_.is_open()) {
        stream_.close();
    }
}

Result<std::unique_ptr<FileSink>> FileSink::create(const std::filesystem::path& path, Mode mode) {
    std::ios_base::openmode open_mode = std::ios::out | std::ios::binary;
    if (mode == Mode::Append) {
        open_mode |= std::ios::app;
    } else {
        open_mode |= std::ios::trunc;
    }

    std::ofstream stream(path, open_mode);
    if (!stream.is_open() || stream.fail()) {
        return make_error(ErrorCode::IoError, "failed to open log file");
    }

    return std::make_unique<FileSink>(ConstructToken{}, std::move(stream));
}

void FileSink::write(const LogRecord& record) {
    const std::string line = format_log_line(record);
    std::lock_guard<std::mutex> lock(mutex_);
    stream_ << line << '\n';
    if (record.level >= LogLevel::Warn && record.level != LogLevel::Off) {
        stream_.flush();
    }
}

void FileSink::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    stream_.flush();
}

}  // namespace nxtcut::core
