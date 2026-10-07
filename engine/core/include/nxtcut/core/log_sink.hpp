#pragma once

#include <nxtcut/core/log_level.hpp>

#include <cstdint>
#include <string_view>

namespace nxtcut::core {

/**
 * @brief Structured record representing an individual logging event.
 *
 * @note Thread safety: Value semantics. The string views are valid ONLY during the sink write() call.
 */
struct LogRecord {
    LogLevel level{LogLevel::Info};
    std::int64_t unix_millis{0};

    /**
     * @brief Name of the logger originating this event.
     * @note Valid only for the duration of the write call.
     */
    std::string_view logger_name;

    /**
     * @brief Formatted message payload.
     * @note Valid only for the duration of the write call.
     */
    std::string_view message;
};

/**
 * @brief Abstract destination sink interface for log events.
 *
 * Sinks process log records delivered from Logger instances. Implementations must guarantee
 * thread-safe operation as write() and flush() can be invoked simultaneously across threads.
 *
 * @note Thread safety: Thread-safe (implementations must be safe to call concurrently from multiple threads).
 */
class LogSink {
public:
    virtual ~LogSink() = default;

    LogSink(const LogSink&) = delete;
    LogSink& operator=(const LogSink&) = delete;
    LogSink(LogSink&&) = delete;
    LogSink& operator=(LogSink&&) = delete;

    /**
     * @brief Writes a single log record to the destination.
     *
     * @param record The log record to output.
     */
    virtual void write(const LogRecord& record) = 0;

    /**
     * @brief Flushes buffered data to the underlying destination.
     */
    virtual void flush() = 0;

protected:
    LogSink() = default;
};

}  // namespace nxtcut::core
