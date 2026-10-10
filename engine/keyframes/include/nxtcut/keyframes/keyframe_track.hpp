#pragma once

#include <nxtcut/core/error.hpp>
#include <nxtcut/core/result.hpp>
#include <nxtcut/core/time.hpp>
#include <nxtcut/keyframes/animatable.hpp>
#include <nxtcut/keyframes/keyframe.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace nxtcut::keyframes {

/**
 * @brief Maximum allowed keyframe tick magnitude (+/- 2^62).
 *
 * Prevents 64-bit integer overflow when calculating time differences between keyframes.
 */
inline constexpr std::int64_t kMaxKeyframeTicks = std::int64_t{1} << 62;

/**
 * @brief Time-ordered sequence of keyframes for an animatable property.
 *
 * Invariants:
 * - Keys are sorted by time in strictly increasing order (no duplicate timestamps).
 * - Every keyframe value is finite (according to AnimatableTraits<T>::is_finite).
 * - Every timestamp is within [-kMaxKeyframeTicks, +kMaxKeyframeTicks].
 *
 * Evaluation:
 * - O(log n) random access via value_at(t).
 * - O(1) amortized sequential playback via value_at(t, Cursor&).
 * - Allocation-free and noexcept on evaluation.
 *
 * @tparam T The animatable payload type satisfying Animatable<T>.
 * @note Thread safety: Thread-compatible. Const methods are safe for concurrent read access.
 */
template <Animatable T>
class KeyframeTrack {
public:
    /**
     * @brief Playback caching cursor for amortized O(1) sequential evaluation.
     */
    struct Cursor {
        std::size_t segment{0};
    };

    KeyframeTrack() = default;

    /**
     * @brief Validates track invariants and creates a KeyframeTrack.
     *
     * Complexity: O(n).
     *
     * @param keys The vector of keyframes.
     * @return KeyframeTrack or InvalidArgument naming the issue and index.
     */
    [[nodiscard]] static core::Result<KeyframeTrack> create(std::vector<Keyframe<T>> keys) {
        for (std::size_t i = 0; i < keys.size(); ++i) {
            const auto& k = keys[i];
            if (k.time.ticks() < -kMaxKeyframeTicks || k.time.ticks() > kMaxKeyframeTicks) {
                return core::make_error(core::ErrorCode::InvalidArgument,
                                        "keyframe time out of range at index " + std::to_string(i));
            }
            if (!AnimatableTraits<T>::is_finite(k.value)) {
                return core::make_error(
                    core::ErrorCode::InvalidArgument,
                    "keyframe value is not finite at index " + std::to_string(i));
            }
            if (i > 0) {
                if (k.time < keys[i - 1].time) {
                    return core::make_error(
                        core::ErrorCode::InvalidArgument,
                        "keyframes not sorted by time at index " + std::to_string(i));
                }
                if (k.time == keys[i - 1].time) {
                    return core::make_error(
                        core::ErrorCode::InvalidArgument,
                        "duplicate keyframe time at index " + std::to_string(i));
                }
            }
        }
        KeyframeTrack track;
        track.keys_ = std::move(keys);
        return track;
    }

    /**
     * @brief Number of keyframes in the track.
     *
     * Complexity: O(1).
     */
    [[nodiscard]] std::size_t size() const noexcept { return keys_.size(); }

    /**
     * @brief Checks whether the track has zero keyframes.
     *
     * Complexity: O(1).
     */
    [[nodiscard]] bool empty() const noexcept { return keys_.empty(); }

    /**
     * @brief Read-only span of the keyframes in time order.
     *
     * Complexity: O(1).
     */
    [[nodiscard]] std::span<const Keyframe<T>> keys() const noexcept { return keys_; }

    /**
     * @brief Searches for a keyframe at the exact given timestamp.
     *
     * Complexity: O(log n).
     *
     * @param t Target timestamp.
     * @return Index if found, std::nullopt otherwise.
     */
    [[nodiscard]] std::optional<std::size_t> find(core::TimePoint t) const noexcept {
        const auto it = std::lower_bound(
            keys_.begin(), keys_.end(), t,
            [](const Keyframe<T>& k, core::TimePoint time) noexcept { return k.time < time; });
        if (it != keys_.end() && it->time == t) {
            return static_cast<std::size_t>(std::distance(keys_.begin(), it));
        }
        return std::nullopt;
    }

    /**
     * @brief Inserts or replaces a keyframe at key.time, preserving sort order.
     *
     * Validates that key.value is finite and key.time is within [-kMaxKeyframeTicks,
     * +kMaxKeyframeTicks].
     *
     * Complexity: O(n).
     *
     * @param key The keyframe to set.
     * @return The index of the set keyframe, or InvalidArgument.
     */
    core::Result<std::size_t> set(Keyframe<T> key) {
        if (key.time.ticks() < -kMaxKeyframeTicks || key.time.ticks() > kMaxKeyframeTicks) {
            return core::make_error(core::ErrorCode::InvalidArgument, "keyframe time out of range");
        }
        if (!AnimatableTraits<T>::is_finite(key.value)) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "keyframe value is not finite");
        }

        const auto it = std::lower_bound(
            keys_.begin(), keys_.end(), key.time,
            [](const Keyframe<T>& k, core::TimePoint t) noexcept { return k.time < t; });

        if (it != keys_.end() && it->time == key.time) {
            *it = std::move(key);
            return static_cast<std::size_t>(std::distance(keys_.begin(), it));
        }

        const auto inserted_it = keys_.insert(it, std::move(key));
        return static_cast<std::size_t>(std::distance(keys_.begin(), inserted_it));
    }

    /**
     * @brief Removes a keyframe at the exact timestamp t.
     *
     * Complexity: O(n).
     *
     * @param t Timestamp of keyframe to remove.
     * @return True if a keyframe was removed, false otherwise.
     */
    bool remove_at(core::TimePoint t) {
        const auto it = std::lower_bound(
            keys_.begin(), keys_.end(), t,
            [](const Keyframe<T>& k, core::TimePoint time) noexcept { return k.time < time; });
        if (it != keys_.end() && it->time == t) {
            keys_.erase(it);
            return true;
        }
        return false;
    }

    /**
     * @brief Evaluates property value at timestamp t using binary search.
     *
     * Complexity: O(log n).
     *
     * @note If empty(), returns AnimatableTraits<T>::default_value(). Callers are expected
     *       to check empty() first.
     */
    [[nodiscard]] T value_at(core::TimePoint t) const noexcept {
        if (keys_.empty()) {
            return AnimatableTraits<T>::default_value();
        }
        if (keys_.size() == 1 || t <= keys_.front().time) {
            return keys_.front().value;
        }
        if (t >= keys_.back().time) {
            return keys_.back().value;
        }

        const auto it = std::upper_bound(
            keys_.begin(), keys_.end(), t,
            [](core::TimePoint time, const Keyframe<T>& k) noexcept { return time < k.time; });
        const std::size_t seg = static_cast<std::size_t>(std::distance(keys_.begin(), it)) - 1;
        return evaluate_segment(seg, t);
    }

    /**
     * @brief Evaluates property value at timestamp t using a playback cursor.
     *
     * Tests cursor's current segment, then next segment, then falls back to binary search.
     * Returns results bit-identical to value_at(t). Stale cursors are handled safely.
     *
     * Complexity: O(1) amortized for sequential playback, O(log n) on jumps.
     *
     * @note If empty(), returns AnimatableTraits<T>::default_value(). Callers are expected
     *       to check empty() first.
     */
    [[nodiscard]] T value_at(core::TimePoint t, Cursor& cursor) const noexcept {
        if (keys_.empty()) {
            cursor.segment = 0;
            return AnimatableTraits<T>::default_value();
        }
        if (keys_.size() == 1) {
            cursor.segment = 0;
            return keys_.front().value;
        }
        if (t <= keys_.front().time) {
            cursor.segment = 0;
            return keys_.front().value;
        }
        if (t >= keys_.back().time) {
            cursor.segment = keys_.size() - 2;
            return keys_.back().value;
        }

        // Fast path 1: current segment
        if (cursor.segment < keys_.size() - 1 && keys_[cursor.segment].time <= t &&
            t < keys_[cursor.segment + 1].time) {
            return evaluate_segment(cursor.segment, t);
        }

        // Fast path 2: next segment
        const std::size_t next_seg = cursor.segment + 1;
        if (next_seg < keys_.size() - 1 && keys_[next_seg].time <= t &&
            t < keys_[next_seg + 1].time) {
            cursor.segment = next_seg;
            return evaluate_segment(next_seg, t);
        }

        // Binary search fallback
        const auto it = std::upper_bound(
            keys_.begin(), keys_.end(), t,
            [](core::TimePoint time, const Keyframe<T>& k) noexcept { return time < k.time; });
        const std::size_t seg = static_cast<std::size_t>(std::distance(keys_.begin(), it)) - 1;
        cursor.segment = seg;
        return evaluate_segment(seg, t);
    }

private:
    [[nodiscard]] T evaluate_segment(std::size_t seg_index, core::TimePoint t) const noexcept {
        const auto& a = keys_[seg_index];
        const auto& b = keys_[seg_index + 1];
        const double p = static_cast<double>(t.ticks() - a.time.ticks()) /
                         static_cast<double>(b.time.ticks() - a.time.ticks());
        const double eased = a.interpolation.map_progress(p);
        if (eased == 0.0) {
            return a.value;
        }
        if (eased == 1.0) {
            return b.value;
        }
        return AnimatableTraits<T>::interpolate(a.value, b.value, eased);
    }

    std::vector<Keyframe<T>> keys_{};
};

/**
 * @brief Tests two KeyframeTrack instances for exact equality.
 */
template <Animatable T>
[[nodiscard]] bool identical(const KeyframeTrack<T>& a, const KeyframeTrack<T>& b) noexcept {
    if (a.size() != b.size()) {
        return false;
    }
    const auto keys_a = a.keys();
    const auto keys_b = b.keys();
    for (std::size_t i = 0; i < keys_a.size(); ++i) {
        if (!identical(keys_a[i], keys_b[i])) {
            return false;
        }
    }
    return true;
}

}  // namespace nxtcut::keyframes
