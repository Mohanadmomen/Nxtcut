#pragma once

#include <nxtcut/core/result.hpp>

#include <cstdint>

namespace nxtcut::core {

/**
 * @brief Rounding strategies supported for checked arithmetic operations.
 */
enum class RoundingMode {
    Floor,    ///< Rounds toward negative infinity.
    Nearest,  ///< Rounds half away from zero.
    Ceil,     ///< Rounds toward positive infinity.
};

/**
 * @brief Multiplies two 64-bit integers and divides by a third using a 128-bit intermediate.
 *
 * Computes round(a * b / c) with exact intermediate precision.
 *
 * @param a First multiplicand.
 * @param b Second multiplicand.
 * @param c Divisor (must not be 0).
 * @param mode Rounding strategy to apply if the quotient has a fractional remainder.
 * @return The rounded 64-bit result, or ErrorCode::InvalidArgument if c == 0,
 *         or ErrorCode::Overflow if the result cannot be represented in a signed 64-bit integer.
 *
 * @note Thread safety: Thread-safe (pure function).
 */
[[nodiscard]] Result<std::int64_t> mul_div(
    std::int64_t a,
    std::int64_t b,
    std::int64_t c,
    RoundingMode mode
) noexcept;

}  // namespace nxtcut::core
