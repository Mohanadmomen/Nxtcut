#include <nxtcut/core/log_level.hpp>

#include <cctype>

namespace nxtcut::core {
namespace {

bool equals_case_insensitive(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

}  // namespace

std::string_view to_string(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::Trace:
            return "TRACE";
        case LogLevel::Debug:
            return "DEBUG";
        case LogLevel::Info:
            return "INFO";
        case LogLevel::Warn:
            return "WARN";
        case LogLevel::Error:
            return "ERROR";
        case LogLevel::Critical:
            return "CRITICAL";
        case LogLevel::Off:
            return "OFF";
    }
    return "UNKNOWN";
}

Result<LogLevel> parse_log_level(std::string_view str) {
    if (equals_case_insensitive(str, "trace")) {
        return LogLevel::Trace;
    }
    if (equals_case_insensitive(str, "debug")) {
        return LogLevel::Debug;
    }
    if (equals_case_insensitive(str, "info")) {
        return LogLevel::Info;
    }
    if (equals_case_insensitive(str, "warn")) {
        return LogLevel::Warn;
    }
    if (equals_case_insensitive(str, "error")) {
        return LogLevel::Error;
    }
    if (equals_case_insensitive(str, "critical")) {
        return LogLevel::Critical;
    }
    if (equals_case_insensitive(str, "off")) {
        return LogLevel::Off;
    }
    return make_error(ErrorCode::InvalidArgument, "unknown log level");
}

}  // namespace nxtcut::core
