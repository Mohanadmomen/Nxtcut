#pragma once

#include <nxtcut/core/result.hpp>

#include <cstdint>
#include <numeric>

namespace nxtcut::model {

/**
 * @brief Represents an exact rational playback speed factor (always positive and reduced).
 *
 * For instance, Speed(2, 1) means media plays at 2x real-time speed, such that
 * a clip spanning D timeline ticks consumes 2 * D source media ticks.
 *
 * @note Thread safety: plain data, no internal synchronization; concurrent const access is safe;
 * copies are independent.
 */
class Speed {
public:
    /**
     * @brief Creates a normalized Speed from strictly positive numerator and denominator.
     *
     * Values are reduced to their simplest fractional form via std::gcd.
     *
     * @param numerator Multiplicand factor (> 0).
     * @param denominator Divisor factor (> 0).
     * @return Normalized Speed, or ErrorCode::InvalidArgument if either term is non-positive.
     */
    [[nodiscard]] static core::Result<Speed> create(std::int64_t numerator,
                                                    std::int64_t denominator) {
        if (numerator <= 0 || denominator <= 0) {
            return core::make_error(
                core::ErrorCode::InvalidArgument,
                "Speed numerator and denominator must both be strictly positive");
        }
        const std::int64_t g = std::gcd(numerator, denominator);
        return Speed(numerator / g, denominator / g);
    }

    /**
     * @brief Normal 1x playback speed (1/1).
     */
    [[nodiscard]] static constexpr Speed normal() noexcept { return Speed(1, 1); }

    /**
     * @brief Default constructor initializes speed to 1x (1/1).
     */
    constexpr Speed() noexcept : numerator_(1), denominator_(1) {}

    [[nodiscard]] constexpr std::int64_t numerator() const noexcept { return numerator_; }

    [[nodiscard]] constexpr std::int64_t denominator() const noexcept { return denominator_; }

    [[nodiscard]] constexpr bool operator==(const Speed&) const noexcept = default;

private:
    constexpr explicit Speed(std::int64_t num, std::int64_t den) noexcept
        : numerator_(num), denominator_(den) {}

    std::int64_t numerator_{1};
    std::int64_t denominator_{1};
};

}  // namespace nxtcut::model
