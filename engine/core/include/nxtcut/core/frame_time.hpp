#pragma once

#include <nxtcut/core/frame_rate.hpp>
#include <nxtcut/core/mul_div.hpp>
#include <nxtcut/core/result.hpp>
#include <nxtcut/core/time.hpp>

#include <compare>
#include <cstdint>

namespace nxtcut::core {

/**
 * @brief Strong type representing a discrete zero-based frame index.
 *
 * @note Thread safety: Thread-safe (immutable value type).
 */
class FrameIndex {
public:
    explicit constexpr FrameIndex(std::int64_t val = 0) noexcept : value_(val) {}

    [[nodiscard]] constexpr std::int64_t value() const noexcept {
        return value_;
    }

    [[nodiscard]] constexpr auto operator<=>(const FrameIndex&) const noexcept = default;
    [[nodiscard]] constexpr bool operator==(const FrameIndex&) const noexcept = default;

    [[nodiscard]] constexpr FrameIndex operator+(std::int64_t offset) const noexcept {
        return FrameIndex(value_ + offset);
    }

    [[nodiscard]] friend constexpr FrameIndex operator+(std::int64_t offset, FrameIndex idx) noexcept {
        return FrameIndex(idx.value_ + offset);
    }

    [[nodiscard]] constexpr FrameIndex operator-(std::int64_t offset) const noexcept {
        return FrameIndex(value_ - offset);
    }

    [[nodiscard]] constexpr std::int64_t operator-(FrameIndex other) const noexcept {
        return value_ - other.value_;
    }

    constexpr FrameIndex& operator+=(std::int64_t offset) noexcept {
        value_ += offset;
        return *this;
    }

    constexpr FrameIndex& operator-=(std::int64_t offset) noexcept {
        value_ -= offset;
        return *this;
    }

    constexpr FrameIndex& operator++() noexcept {
        ++value_;
        return *this;
    }

    constexpr FrameIndex operator++(int) noexcept {
        FrameIndex tmp = *this;
        ++value_;
        return tmp;
    }

    constexpr FrameIndex& operator--() noexcept {
        --value_;
        return *this;
    }

    constexpr FrameIndex operator--(int) noexcept {
        FrameIndex tmp = *this;
        --value_;
        return tmp;
    }

private:
    std::int64_t value_{0};
};

/**
 * @brief Converts an absolute frame index to master timeline time ticks.
 *
 * Evaluated as (frame * denominator * kTicksPerSecond / numerator) with Nearest rounding.
 * Guarantees zero drift over arbitrary timeline durations.
 */
[[nodiscard]] Result<TimePoint> frame_to_time(FrameIndex frame, FrameRate rate) noexcept;

/**
 * @brief Converts master timeline time ticks to a discrete frame index using specified rounding.
 */
[[nodiscard]] Result<FrameIndex> time_to_frame(
    TimePoint time,
    FrameRate rate,
    RoundingMode mode
) noexcept;

/**
 * @brief Computes the exact duration in timeline ticks for a single frame at the given rate.
 */
[[nodiscard]] Result<Duration> frame_duration(FrameRate rate) noexcept;

/**
 * @brief Snaps an arbitrary timeline timestamp to the nearest discrete frame boundary.
 */
[[nodiscard]] Result<TimePoint> snap_to_frame(
    TimePoint time,
    FrameRate rate,
    RoundingMode mode
) noexcept;

/**
 * @brief Converts an audio sample count to master timeline time ticks.
 */
[[nodiscard]] Result<TimePoint> samples_to_time(
    std::int64_t samples,
    SampleRate rate
) noexcept;

/**
 * @brief Converts master timeline time ticks to an audio sample count using specified rounding.
 */
[[nodiscard]] Result<std::int64_t> time_to_samples(
    TimePoint time,
    SampleRate rate,
    RoundingMode mode
) noexcept;

}  // namespace nxtcut::core
