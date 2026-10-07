#pragma once

#include <nxtcut/core/log_sink.hpp>

#include <string>

namespace nxtcut::core {

/**
 * @brief Formats a LogRecord into a canonical UTC log line.
 *
 * The output format is:
 *   YYYY-MM-DD HH:MM:SS.mmm LEVEL [logger] message
 * Note: If message is empty, the formatted line ends with a single trailing space after `]`.
 *
 * @param record Log record to format.
 * @return Formatted log line without trailing newline.
 * @note Thread safety: Thread-safe (pure re-entrant function).
 */
[[nodiscard]] std::string format_log_line(const LogRecord& record);

}  // namespace nxtcut::core
