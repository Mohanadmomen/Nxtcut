#pragma once

#include <nxtcut/core/result.hpp>

#include <compare>
#include <cstdint>
#include <string>

namespace nxtcut::core {

class FrameRate;
struct FrameRateInternalAccess;

/**
 * @brief Represents a rational frame rate specified as a positive numerator and denominator.
 *
 * All FrameRate instances created via create() are reduced to simplest fractional form using
 * std::gcd. Comparisons are computed using exact 64-bit cross-multiplication with no floating-point
 * inaccuracies.
 *
 * @note Thread safety: Thread-safe (immutable value type).
 */
class FrameRate {
public:
    /**
     * @brief Creates a normalized FrameRate from positive numerator and denominator.
     */
    [[nodiscard]] static Result<FrameRate> create(std::int32_t num, std::int32_t den);

    [[nodiscard]] constexpr std::int32_t numerator() const noexcept { return numerator_; }

    [[nodiscard]] constexpr std::int32_t denominator() const noexcept { return denominator_; }

    [[nodiscard]] constexpr double to_double() const noexcept {
        return static_cast<double>(numerator_) / static_cast<double>(denominator_);
    }

    /**
     * @brief Converts the frame rate to standard display string (e.g. "29.97", "24", "23.976").
     */
    [[nodiscard]] std::string to_string() const;

    /**
     * @brief Computes the nearest integer frames-per-second (e.g. 30 for 29.97, 24 for 23.976).
     */
    [[nodiscard]] constexpr std::int32_t nominal_fps() const noexcept {
        return static_cast<std::int32_t>(
            (static_cast<std::int64_t>(numerator_) + denominator_ / 2) / denominator_);
    }

    /**
     * @brief Checks if this is an NTSC standard rate (denominator == 1001).
     */
    [[nodiscard]] constexpr bool is_ntsc() const noexcept { return denominator_ == 1001; }

    [[nodiscard]] constexpr std::strong_ordering operator<=>(
        const FrameRate& other) const noexcept {
        const std::int64_t lhs = static_cast<std::int64_t>(numerator_) * other.denominator_;
        const std::int64_t rhs = static_cast<std::int64_t>(other.numerator_) * denominator_;
        return lhs <=> rhs;
    }

    [[nodiscard]] constexpr bool operator==(const FrameRate& other) const noexcept {
        return (*this <=> other) == 0;
    }

private:
    constexpr explicit FrameRate(std::int32_t num, std::int32_t den) noexcept
        : numerator_(num), denominator_(den) {}

    friend struct FrameRateInternalAccess;

    std::int32_t numerator_{30};
    std::int32_t denominator_{1};
};

struct FrameRateInternalAccess {
    static constexpr FrameRate make(std::int32_t num, std::int32_t den) noexcept {
        return FrameRate(num, den);
    }
};

namespace frame_rates {
inline constexpr FrameRate k23_976 = FrameRateInternalAccess::make(24000, 1001);
inline constexpr FrameRate k24 = FrameRateInternalAccess::make(24, 1);
inline constexpr FrameRate k25 = FrameRateInternalAccess::make(25, 1);
inline constexpr FrameRate k29_97 = FrameRateInternalAccess::make(30000, 1001);
inline constexpr FrameRate k30 = FrameRateInternalAccess::make(30, 1);
inline constexpr FrameRate k50 = FrameRateInternalAccess::make(50, 1);
inline constexpr FrameRate k59_94 = FrameRateInternalAccess::make(60000, 1001);
inline constexpr FrameRate k60 = FrameRateInternalAccess::make(60, 1);
}  // namespace frame_rates

class SampleRate;
struct SampleRateInternalAccess;

/**
 * @brief Represents an audio sampling rate in Hertz (samples per second).
 *
 * @note Thread safety: Thread-safe (immutable value type).
 */
class SampleRate {
public:
    static const SampleRate k44100;
    static const SampleRate k48000;
    static const SampleRate k96000;

    /**
     * @brief Creates a SampleRate validated to be strictly positive.
     */
    [[nodiscard]] static Result<SampleRate> create(std::int32_t value);

    [[nodiscard]] constexpr std::int32_t value() const noexcept { return value_; }

    [[nodiscard]] constexpr auto operator<=>(const SampleRate&) const noexcept = default;
    [[nodiscard]] constexpr bool operator==(const SampleRate&) const noexcept = default;

private:
    constexpr explicit SampleRate(std::int32_t value) noexcept : value_(value) {}

    friend struct SampleRateInternalAccess;

    std::int32_t value_{48000};
};

struct SampleRateInternalAccess {
    static constexpr SampleRate make(std::int32_t value) noexcept { return SampleRate(value); }
};

namespace sample_rates {
inline constexpr SampleRate k44100 = SampleRateInternalAccess::make(44100);
inline constexpr SampleRate k48000 = SampleRateInternalAccess::make(48000);
inline constexpr SampleRate k96000 = SampleRateInternalAccess::make(96000);
}  // namespace sample_rates

inline const SampleRate SampleRate::k44100 = sample_rates::k44100;
inline const SampleRate SampleRate::k48000 = sample_rates::k48000;
inline const SampleRate SampleRate::k96000 = sample_rates::k96000;

}  // namespace nxtcut::core
