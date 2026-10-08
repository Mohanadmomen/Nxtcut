#pragma once

#include <nxtcut/core/time.hpp>

#include <compare>
#include <cstdint>

namespace nxtcut::model {

/**
 * @brief Represents an absolute position along a sequence timeline.
 *
 * @note Thread safety: plain data, no internal synchronization; concurrent const access is safe;
 * copies are independent.
 */
class TimelineTime {
public:
    constexpr TimelineTime() noexcept = default;

    explicit constexpr TimelineTime(core::TimePoint tp) noexcept : tp_(tp) {}

    [[nodiscard]] static constexpr TimelineTime from_ticks(std::int64_t t) noexcept {
        return TimelineTime(core::TimePoint::from_ticks(t));
    }

    [[nodiscard]] static constexpr TimelineTime zero() noexcept {
        return TimelineTime(core::TimePoint::zero());
    }

    [[nodiscard]] constexpr std::int64_t ticks() const noexcept { return tp_.ticks(); }

    [[nodiscard]] constexpr core::TimePoint to_core() const noexcept { return tp_; }

    [[nodiscard]] constexpr auto operator<=>(const TimelineTime&) const noexcept = default;
    [[nodiscard]] constexpr bool operator==(const TimelineTime&) const noexcept = default;

private:
    core::TimePoint tp_{core::TimePoint::zero()};
};

/**
 * @brief Represents an offset measured from the start of a clip, expressed in timeline ticks.
 *
 * @note Thread safety: plain data, no internal synchronization; concurrent const access is safe;
 * copies are independent.
 */
class ClipTime {
public:
    constexpr ClipTime() noexcept = default;

    explicit constexpr ClipTime(core::TimePoint tp) noexcept : tp_(tp) {}

    [[nodiscard]] static constexpr ClipTime from_ticks(std::int64_t t) noexcept {
        return ClipTime(core::TimePoint::from_ticks(t));
    }

    [[nodiscard]] static constexpr ClipTime zero() noexcept {
        return ClipTime(core::TimePoint::zero());
    }

    [[nodiscard]] constexpr std::int64_t ticks() const noexcept { return tp_.ticks(); }

    [[nodiscard]] constexpr core::TimePoint to_core() const noexcept { return tp_; }

    [[nodiscard]] constexpr auto operator<=>(const ClipTime&) const noexcept = default;
    [[nodiscard]] constexpr bool operator==(const ClipTime&) const noexcept = default;

private:
    core::TimePoint tp_{core::TimePoint::zero()};
};

/**
 * @brief Represents a position inside the underlying source media or nested sequence.
 *
 * @note Thread safety: plain data, no internal synchronization; concurrent const access is safe;
 * copies are independent.
 */
class SourceTime {
public:
    constexpr SourceTime() noexcept = default;

    explicit constexpr SourceTime(core::TimePoint tp) noexcept : tp_(tp) {}

    [[nodiscard]] static constexpr SourceTime from_ticks(std::int64_t t) noexcept {
        return SourceTime(core::TimePoint::from_ticks(t));
    }

    [[nodiscard]] static constexpr SourceTime zero() noexcept {
        return SourceTime(core::TimePoint::zero());
    }

    [[nodiscard]] constexpr std::int64_t ticks() const noexcept { return tp_.ticks(); }

    [[nodiscard]] constexpr core::TimePoint to_core() const noexcept { return tp_; }

    [[nodiscard]] constexpr auto operator<=>(const SourceTime&) const noexcept = default;
    [[nodiscard]] constexpr bool operator==(const SourceTime&) const noexcept = default;

private:
    core::TimePoint tp_{core::TimePoint::zero()};
};

}  // namespace nxtcut::model
