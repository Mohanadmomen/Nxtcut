#include <nxtcut/core/console_sink.hpp>
#include <nxtcut/core/log_format.hpp>

namespace nxtcut::core {

ConsoleSink::ConsoleSink(std::ostream& out) : out_(out) {}

void ConsoleSink::write(const LogRecord& record) {
    const std::string line = format_log_line(record);
    std::lock_guard<std::mutex> lock(mutex_);
    out_ << line << '\n';
}

void ConsoleSink::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    out_.flush();
}

}  // namespace nxtcut::core
