#pragma once

#include <string>
#include <string_view>

namespace nxtcut::core {

/**
 * @brief Categorized error codes used across the NxtCut engine.
 */
enum class ErrorCode {
    InvalidArgument,
    OutOfRange,
    Overflow,
    NotFound,
    AlreadyExists,
    IoError,
    Corrupt,
    Unsupported,
    Cancelled,
    Internal,
};

/**
 * @brief Converts an ErrorCode to a human-readable string representation.
 *
 * @param code The error code to convert.
 * @return String representation of the error code name.
 * @note Thread safety: Thread-safe (re-entrant pure function).
 */
[[nodiscard]] std::string_view to_string(ErrorCode code) noexcept;

/**
 * @brief Represents an operational failure with an ErrorCode and explanatory message.
 *
 * @note Thread safety: Thread-safe (immutable data type with value semantics).
 */
class Error {
public:
    /**
     * @brief Constructs an Error with a code and an optional message.
     *
     * @param code The error classification code.
     * @param message Detailed context or reason for the failure.
     */
    explicit Error(ErrorCode code, std::string message = {}) noexcept;

    /**
     * @brief Gets the error classification code.
     */
    [[nodiscard]] ErrorCode code() const noexcept { return code_; }

    /**
     * @brief Gets the explanatory error message.
     */
    [[nodiscard]] std::string_view message() const noexcept { return message_; }

    /**
     * @brief Produces a new Error with prepended contextual information.
     *
     * The resulting error will have message "context: original_message".
     *
     * @param context Additional contextual description to prepend.
     * @return A new Error instance with the augmented message.
     */
    [[nodiscard]] Error with_context(std::string_view context) const;

    /**
     * @brief Formats this Error as a human-readable string.
     *
     * @return Formatted string containing error code name and message.
     */
    [[nodiscard]] std::string to_string() const;

    /**
     * @brief Value equality comparison checking both code and message.
     */
    [[nodiscard]] bool operator==(const Error& other) const noexcept = default;

private:
    ErrorCode code_;
    std::string message_;
};

}  // namespace nxtcut::core
