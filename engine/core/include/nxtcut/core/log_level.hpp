#pragma once

#include <nxtcut/core/result.hpp>

#include <cstdint>
#include <string_view>

namespace nxtcut::core {

/**
 * @brief Severity levels for log records, ordered from Trace (lowest) to Off (highest).
 */
enum class LogLevel : std::uint8_t {
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    Critical,
    Off,
};

/**
 * @brief Converts a LogLevel to an uppercase string without padding.
 *
 * @param level Severity level to convert.
 * @return String representation ("TRACE", "DEBUG", "INFO", "WARN", "ERROR", "CRITICAL", "OFF").
 * @note Thread safety: Thread-safe (pure re-entrant function).
 */
[[nodiscard]] std::string_view to_string(LogLevel level) noexcept;

/**
 * @brief Parses a case-insensitive log level string.
 *
 * @param str Input string matching one of the canonical level names.
 * @return The parsed LogLevel, or ErrorCode::InvalidArgument if unrecognized.
 * @note Thread safety: Thread-safe (pure re-entrant function).
 */
[[nodiscard]] Result<LogLevel> parse_log_level(std::string_view str);

}  // namespace nxtcut::core
