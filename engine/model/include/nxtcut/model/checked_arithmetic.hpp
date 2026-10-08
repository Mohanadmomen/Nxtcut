#pragma once

#include <cstdint>
#include <limits>
#include <optional>

namespace nxtcut::model::detail {

/**
 * @brief Performs portable overflow-checked 64-bit signed integer addition.
 *
 * @param a First operand.
 * @param b Second operand.
 * @return Sum of a and b, or std::nullopt if the mathematical result overflows std::int64_t bounds.
 */
[[nodiscard]] constexpr std::optional<std::int64_t> checked_add(std::int64_t a,
                                                                std::int64_t b) noexcept {
    if ((b > 0 && a > std::numeric_limits<std::int64_t>::max() - b) ||
        (b < 0 && a < std::numeric_limits<std::int64_t>::min() - b)) {
        return std::nullopt;
    }
    return a + b;
}

/**
 * @brief Performs portable overflow-checked 64-bit signed integer subtraction.
 *
 * @param a Minuend.
 * @param b Subtrahend.
 * @return Difference of a - b, or std::nullopt if the mathematical result overflows std::int64_t
 * bounds.
 */
[[nodiscard]] constexpr std::optional<std::int64_t> checked_sub(std::int64_t a,
                                                                std::int64_t b) noexcept {
    if ((b > 0 && a < std::numeric_limits<std::int64_t>::min() + b) ||
        (b < 0 && a > std::numeric_limits<std::int64_t>::max() + b)) {
        return std::nullopt;
    }
    return a - b;
}

}  // namespace nxtcut::model::detail
