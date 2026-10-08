#pragma once

#include <nxtcut/core/mul_div.hpp>
#include <nxtcut/core/result.hpp>

#include <cstdint>

namespace nxtcut::core::detail {

/**
 * @brief Software-only 128-bit portable fallback implementation of mul_div.
 *
 * Computes round(a * b / c) with exact intermediate precision using portable
 * multi-precision arithmetic without platform-specific 128-bit intrinsics.
 *
 * @note INTERNAL: exposed only so tests can compare it with the fast path; use
 *       nxtcut::core::mul_div.
 * @note Thread safety: Thread-safe (pure function).
 *
 * @param a First multiplicand.
 * @param b Second multiplicand.
 * @param c Divisor (must not be 0).
 * @param mode Rounding strategy to apply if the quotient has a fractional remainder.
 * @return The rounded 64-bit result, or ErrorCode::InvalidArgument if c == 0,
 *         or ErrorCode::Overflow if the result cannot be represented in a signed 64-bit integer.
 */
[[nodiscard]] Result<std::int64_t> mul_div_portable(std::int64_t a, std::int64_t b, std::int64_t c,
                                                    RoundingMode mode) noexcept;

}  // namespace nxtcut::core::detail
