#include <nxtcut/core/logger.hpp>

#include <chrono>
#include <string>
#include <utility>

namespace nxtcut::core {

Logger::Logger(std::string name,
               LogLevel min_level,
               std::vector<std::shared_ptr<LogSink>> sinks,
               std::function<std::int64_t()> clock)
    : name_(std::move(name))
    , level_(static_cast<std::uint8_t>(min_level))
    , clock_(std::move(clock)) {
    sinks_.reserve(sinks.size());
    for (auto& s : sinks) {
        if (s != nullptr) {
            sinks_.push_back(std::move(s));
        }
    }
    if (!clock_) {
        clock_ = []() -> std::int64_t {
            const auto now = std::chrono::system_clock::now();
            return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        };
    }
}

Logger::Logger(const Logger& other)
    : name_(other.name_)
    , level_(other.level_.load(std::memory_order_relaxed))
    , sinks_(other.sinks_)
    , clock_(other.clock_) {}

Logger& Logger::operator=(const Logger& other) {
    if (this != &other) {
        name_ = other.name_;
        level_.store(other.level_.load(std::memory_order_relaxed), std::memory_order_relaxed);
        sinks_ = other.sinks_;
        clock_ = other.clock_;
    }
    return *this;
}

Logger::Logger(Logger&& other) noexcept
    : name_(std::move(other.name_))
    , level_(other.level_.load(std::memory_order_relaxed))
    , sinks_(std::move(other.sinks_))
    , clock_(std::move(other.clock_)) {}

Logger& Logger::operator=(Logger&& other) noexcept {
    if (this != &other) {
        name_ = std::move(other.name_);
        level_.store(other.level_.load(std::memory_order_relaxed), std::memory_order_relaxed);
        sinks_ = std::move(other.sinks_);
        clock_ = std::move(other.clock_);
    }
    return *this;
}

bool Logger::should_log(LogLevel level) const noexcept {
    if (level == LogLevel::Off) {
        return false;
    }
    const LogLevel current = this->level();
    if (current == LogLevel::Off) {
        return false;
    }
    return static_cast<std::uint8_t>(level) >= static_cast<std::uint8_t>(current);
}

void Logger::set_level(LogLevel level) noexcept {
    level_.store(static_cast<std::uint8_t>(level), std::memory_order_relaxed);
}

LogLevel Logger::level() const noexcept {
    return static_cast<LogLevel>(level_.load(std::memory_order_relaxed));
}

void Logger::log(LogLevel level, std::string_view message) const {
    if (!should_log(level)) {
        return;
    }
    const std::int64_t now_ms = clock_ ? clock_() : 0;
    const LogRecord record{level, now_ms, name_, message};
    for (const auto& sink : sinks_) {
        sink->write(record);
    }
}

Logger Logger::child(std::string_view suffix) const {
    const std::string child_name = name_.empty() ? std::string(suffix) : name_ + "." + std::string(suffix);
    return Logger(child_name, level(), sinks_, clock_);
}

}  // namespace nxtcut::core
