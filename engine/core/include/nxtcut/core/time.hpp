#pragma once

#include <nxtcut/core/result.hpp>

#include <compare>
#include <cstdint>
#include <optional>

namespace nxtcut::core {

/**
 * @brief Master timeline clock resolution: 705,600,000 ticks per second.
 *
 * This frequency is chosen because it is evenly divisible by all standard video frame rates
 * (24, 25, 30, 48, 50, 60, NTSC 24000/1001, 30000/1001, 60000/1001) as well as standard audio
 * sample rates (44.1 kHz, 48 kHz, 96 kHz, 192 kHz), enabling exact rational time representation
 * without accumulating floating-point drift.
 */
inline constexpr std::int64_t kTicksPerSecond = 705'600'000;

class Duration;

/**
 * @brief Represents an absolute instant on the timeline measured in integer ticks since zero.
 *
 * @note Thread safety: Thread-safe (immutable value type).
 * @note Arithmetic overflow: Callers are responsible for preventing tick overflow.
 *       With 64-bit signed ticks, range is approximately +/- 414 years.
 */
class TimePoint {
public:
    constexpr TimePoint() noexcept : ticks_(0) {}

    [[nodiscard]] static constexpr TimePoint from_ticks(std::int64_t ticks) noexcept {
        return TimePoint(ticks);
    }

    [[nodiscard]] static constexpr TimePoint from_seconds(double seconds) noexcept {
        const double scaled = seconds * static_cast<double>(kTicksPerSecond);
        const double rounded = (scaled >= 0.0) ? (scaled + 0.5) : (scaled - 0.5);
        return TimePoint(static_cast<std::int64_t>(rounded));
    }

    [[nodiscard]] static constexpr TimePoint zero() noexcept { return TimePoint(0); }

    [[nodiscard]] constexpr std::int64_t ticks() const noexcept { return ticks_; }

    [[nodiscard]] constexpr double to_seconds() const noexcept {
        return static_cast<double>(ticks_) / static_cast<double>(kTicksPerSecond);
    }

    [[nodiscard]] constexpr auto operator<=>(const TimePoint&) const noexcept = default;
    [[nodiscard]] constexpr bool operator==(const TimePoint&) const noexcept = default;

    [[nodiscard]] constexpr Duration operator-(TimePoint other) const noexcept;
    [[nodiscard]] constexpr TimePoint operator+(Duration d) const noexcept;
    [[nodiscard]] constexpr TimePoint operator-(Duration d) const noexcept;

    constexpr TimePoint& operator+=(Duration d) noexcept;
    constexpr TimePoint& operator-=(Duration d) noexcept;

private:
    explicit constexpr TimePoint(std::int64_t ticks) noexcept : ticks_(ticks) {}

    std::int64_t ticks_{0};
};

/**
 * @brief Represents a relative temporal displacement measured in integer ticks.
 *
 * @note Thread safety: Thread-safe (immutable value type).
 * @note Arithmetic overflow: Callers are responsible for preventing tick overflow.
 */
class Duration {
public:
    constexpr Duration() noexcept : ticks_(0) {}

    [[nodiscard]] static constexpr Duration from_ticks(std::int64_t ticks) noexcept {
        return Duration(ticks);
    }

    [[nodiscard]] static constexpr Duration from_seconds(double seconds) noexcept {
        const double scaled = seconds * static_cast<double>(kTicksPerSecond);
        const double rounded = (scaled >= 0.0) ? (scaled + 0.5) : (scaled - 0.5);
        return Duration(static_cast<std::int64_t>(rounded));
    }

    [[nodiscard]] static constexpr Duration zero() noexcept { return Duration(0); }

    [[nodiscard]] constexpr std::int64_t ticks() const noexcept { return ticks_; }

    [[nodiscard]] constexpr double to_seconds() const noexcept {
        return static_cast<double>(ticks_) / static_cast<double>(kTicksPerSecond);
    }

    [[nodiscard]] constexpr auto operator<=>(const Duration&) const noexcept = default;
    [[nodiscard]] constexpr bool operator==(const Duration&) const noexcept = default;

    [[nodiscard]] constexpr Duration operator+() const noexcept { return *this; }

    [[nodiscard]] constexpr Duration operator-() const noexcept { return Duration(-ticks_); }

    [[nodiscard]] constexpr Duration operator+(Duration other) const noexcept {
        return Duration(ticks_ + other.ticks_);
    }

    [[nodiscard]] constexpr Duration operator-(Duration other) const noexcept {
        return Duration(ticks_ - other.ticks_);
    }

    [[nodiscard]] constexpr Duration operator*(std::int64_t scalar) const noexcept {
        return Duration(ticks_ * scalar);
    }

    [[nodiscard]] friend constexpr Duration operator*(std::int64_t scalar, Duration d) noexcept {
        return Duration(scalar * d.ticks_);
    }

    [[nodiscard]] constexpr Duration operator/(std::int64_t scalar) const noexcept {
        return Duration(ticks_ / scalar);
    }

    constexpr Duration& operator+=(Duration other) noexcept {
        ticks_ += other.ticks_;
        return *this;
    }

    constexpr Duration& operator-=(Duration other) noexcept {
        ticks_ -= other.ticks_;
        return *this;
    }

    constexpr Duration& operator*=(std::int64_t scalar) noexcept {
        ticks_ *= scalar;
        return *this;
    }

    constexpr Duration& operator/=(std::int64_t scalar) noexcept {
        ticks_ /= scalar;
        return *this;
    }

    [[nodiscard]] constexpr Duration abs() const noexcept {
        return Duration(ticks_ < 0 ? -ticks_ : ticks_);
    }

    [[nodiscard]] friend constexpr Duration abs(Duration d) noexcept { return d.abs(); }

private:
    explicit constexpr Duration(std::int64_t ticks) noexcept : ticks_(ticks) {}

    std::int64_t ticks_{0};
};

inline constexpr Duration TimePoint::operator-(TimePoint other) const noexcept {
    return Duration::from_ticks(ticks_ - other.ticks_);
}

inline constexpr TimePoint TimePoint::operator+(Duration d) const noexcept {
    return TimePoint::from_ticks(ticks_ + d.ticks());
}

inline constexpr TimePoint operator+(Duration d, TimePoint tp) noexcept {
    return TimePoint::from_ticks(tp.ticks() + d.ticks());
}

inline constexpr TimePoint TimePoint::operator-(Duration d) const noexcept {
    return TimePoint::from_ticks(ticks_ - d.ticks());
}

inline constexpr TimePoint& TimePoint::operator+=(Duration d) noexcept {
    ticks_ += d.ticks();
    return *this;
}

inline constexpr TimePoint& TimePoint::operator-=(Duration d) noexcept {
    ticks_ -= d.ticks();
    return *this;
}

/**
 * @brief Represents a half-open temporal interval [start, start + duration).
 *
 * @note Thread safety: Thread-safe (immutable value type).
 */
class TimeRange {
public:
    constexpr TimeRange() noexcept = default;

    /**
     * @brief Creates a TimeRange from a start point and non-negative duration.
     */
    [[nodiscard]] static Result<TimeRange> create(TimePoint start, Duration d);

    /**
     * @brief Creates a TimeRange spanning [start, end). Fails if end < start.
     */
    [[nodiscard]] static Result<TimeRange> from_start_end(TimePoint start, TimePoint end);

    [[nodiscard]] constexpr TimePoint start() const noexcept { return start_; }

    [[nodiscard]] constexpr Duration duration() const noexcept { return duration_; }

    [[nodiscard]] constexpr TimePoint end() const noexcept { return start_ + duration_; }

    /**
     * @brief Tests whether this interval contains the given point (start inclusive, end exclusive).
     */
    [[nodiscard]] constexpr bool contains(TimePoint tp) const noexcept {
        return tp >= start_ && tp < end();
    }

    /**
     * @brief Tests whether this interval completely encloses another range.
     */
    [[nodiscard]] constexpr bool contains(TimeRange other) const noexcept {
        return other.start() >= start_ && other.end() <= end();
    }

    /**
     * @brief Tests whether this interval overlaps with another. Touching ranges do not overlap.
     */
    [[nodiscard]] constexpr bool overlaps(TimeRange other) const noexcept {
        const TimePoint max_start = (start_ > other.start_) ? start_ : other.start_;
        const TimePoint min_end = (end() < other.end()) ? end() : other.end();
        return max_start < min_end;
    }

    /**
     * @brief Computes the intersection of two intervals, returning nullopt if they do not overlap.
     */
    [[nodiscard]] constexpr std::optional<TimeRange> intersection(TimeRange other) const noexcept {
        const TimePoint max_start = (start_ > other.start_) ? start_ : other.start_;
        const TimePoint min_end = (end() < other.end()) ? end() : other.end();
        if (max_start < min_end) {
            return TimeRange(max_start, min_end - max_start);
        }
        return std::nullopt;
    }

    [[nodiscard]] constexpr bool operator==(const TimeRange&) const noexcept = default;

private:
    constexpr TimeRange(TimePoint start, Duration d) noexcept : start_(start), duration_(d) {}

    TimePoint start_{TimePoint::zero()};
    Duration duration_{Duration::zero()};
};

}  // namespace nxtcut::core
