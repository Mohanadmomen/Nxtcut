#pragma once

#include <nxtcut/core/log_sink.hpp>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace nxtcut::core {

/**
 * @brief Thread-safe logger delivering structured records to registered sinks.
 *
 * There is no global logger or static singleton instance. Callers instantiate loggers
 * explicitly and inject them into components that require logging capabilities.
 * Sinks are held via std::shared_ptr because multiple loggers legitimately share one sink.
 *
 * @note Thread safety: Logging, should_log, level and set_level may be called concurrently from any
 * thread. Copy/move construction and copy/move assignment are NOT synchronized: do not assign to or
 * move from a Logger while another thread is using it.
 */
class Logger {
public:
    /**
     * @brief Constructs a Logger with an explicit name, threshold level, sinks, and optional clock.
     *
     * @param name Descriptive hierarchy or component name.
     * @param min_level Minimum severity required to emit records.
     * @param sinks Collection of destination sinks. Null pointers are filtered out.
     * @param clock Optional custom timestamp provider returning unix milliseconds. Defaults to
     * system_clock.
     */
    Logger(std::string name, LogLevel min_level, std::vector<std::shared_ptr<LogSink>> sinks,
           std::function<std::int64_t()> clock = {});

    Logger(const Logger& other);
    Logger& operator=(const Logger& other);
    Logger(Logger&& other) noexcept;
    Logger& operator=(Logger&& other) noexcept;
    ~Logger() = default;

    /**
     * @brief Gets the logger name.
     */
    [[nodiscard]] const std::string& name() const noexcept { return name_; }

    /**
     * @brief Queries whether messages at the given level will be delivered.
     */
    [[nodiscard]] bool should_log(LogLevel level) const noexcept;

    /**
     * @brief Sets the minimum severity threshold.
     */
    void set_level(LogLevel level) noexcept;

    /**
     * @brief Gets the current severity threshold.
     */
    [[nodiscard]] LogLevel level() const noexcept;

    /**
     * @brief Emits a log record at the given severity level.
     */
    void log(LogLevel level, std::string_view message) const;

    void trace(std::string_view message) const { log(LogLevel::Trace, message); }
    void debug(std::string_view message) const { log(LogLevel::Debug, message); }
    void info(std::string_view message) const { log(LogLevel::Info, message); }
    void warn(std::string_view message) const { log(LogLevel::Warn, message); }
    void error(std::string_view message) const { log(LogLevel::Error, message); }
    void critical(std::string_view message) const { log(LogLevel::Critical, message); }

    /**
     * @brief Creates a child logger with hierarchical name "<name>.<suffix>".
     *
     * Shares the same sinks, clock provider, and current log level.
     *
     * @param suffix Child component name suffix.
     * @return Child Logger instance.
     */
    [[nodiscard]] Logger child(std::string_view suffix) const;

private:
    std::string name_;
    std::atomic<std::uint8_t> level_{static_cast<std::uint8_t>(LogLevel::Info)};
    // shared_ptr for sinks is justified because several loggers legitimately share one sink.
    std::vector<std::shared_ptr<LogSink>> sinks_;
    std::function<std::int64_t()> clock_;
};

}  // namespace nxtcut::core
