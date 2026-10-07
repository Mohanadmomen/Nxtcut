#pragma once

#include <nxtcut/core/error.hpp>

#include <string>
#include <string_view>
#include <utility>

#include <tl/expected.hpp>

namespace nxtcut::core {

/**
 * @brief Value-or-error return type used across the NxtCut engine.
 *
 * @tparam T The success payload type.
 */
template <class T>
using Result = tl::expected<T, Error>;

/**
 * @brief Result alias for operations that do not produce a return value on success.
 */
using Status = Result<void>;

/**
 * @brief Helper function to construct an unexpected Error for return in Result<T>.
 *
 * @param code The error classification code.
 * @param message Explanatory message.
 * @return An unexpected error wrapper compatible with Result<T>.
 */
[[nodiscard]] inline tl::unexpected<Error> make_error(ErrorCode code, std::string_view message) {
    return tl::unexpected<Error>(Error(code, std::string(message)));
}

}  // namespace nxtcut::core
