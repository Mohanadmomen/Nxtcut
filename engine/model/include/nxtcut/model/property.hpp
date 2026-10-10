#pragma once

#include <nxtcut/keyframes/animatable.hpp>
#include <nxtcut/keyframes/keyframe_track.hpp>
#include <nxtcut/model/time_coords.hpp>

#include <concepts>
#include <memory>
#include <type_traits>
#include <utility>

namespace nxtcut::model {

namespace detail {

/**
 * @brief Conditionally stores KeyframeTrack shared ownership for animatable types.
 *
 * For non-animatable types (e.g. bool, std::string), this is an empty type.
 */
template <class T, bool = keyframes::Animatable<T>>
struct PropertyTrackStorage {
    [[nodiscard]] static constexpr bool is_animated() noexcept { return false; }
};

template <class T>
struct PropertyTrackStorage<T, true> {
    std::shared_ptr<const keyframes::KeyframeTrack<T>> track{nullptr};

    [[nodiscard]] bool is_animated() const noexcept { return track != nullptr; }
};

}  // namespace detail

/**
 * @brief Represents an animatable value parameter.
 *
 * Holds a constant value and, for animatable payload types (satisfying
 * keyframes::Animatable<T>), an optional immutable keyframe track via
 * std::shared_ptr<const keyframes::KeyframeTrack<T>> (null indicates not animated).
 *
 * Semantics:
 * - The constant value is always stored and preserved even when animated.
 * - When animated, value_at() evaluates using the keyframe track.
 * - Calling set_constant() while animated modifies only the constant value.
 * - Calling clear_keyframes() resets the track and restores constant evaluation.
 * - Setting an empty track via set_keyframes() clears animation (shared_ptr reset).
 * - Keyframe tracks are immutable and shared across Property copies (copy-on-write).
 * - Non-animatable types (e.g. bool, std::string) remain constant-only with zero track storage
 * overhead.
 *
 * Thread safety:
 * - Thread-compatible. Concurrent const access is safe as tracks are immutable and shared.
 *   Mutating a Property is not thread-safe and requires external synchronization.
 *
 * Complexity of value_at:
 * - Constant (non-animated): O(1).
 * - Animated: O(log n) random access.
 * - Animated with Cursor: O(1) amortized for sequential playback.
 *
 * @tparam T The parameter payload value type (e.g. double, bool, std::string, core::Color).
 */
template <class T>
class Property {
public:
    /**
     * @brief Constructs a Property with default-initialized constant value.
     */
    Property() : constant_{} {}

    /**
     * @brief Constructs a Property with an initial constant value.
     *
     * @param constant The initial constant value.
     */
    explicit Property(T constant) : constant_(std::move(constant)) {}

    /**
     * @brief Returns the stored constant value.
     */
    [[nodiscard]] const T& constant_value() const noexcept { return constant_; }

    /**
     * @brief Updates the stored constant value.
     *
     * @note If the property is currently animated, this updates the constant without
     * affecting keyframe evaluation until clear_keyframes() is called.
     */
    void set_constant(T constant) { constant_ = std::move(constant); }

    /**
     * @brief Checks whether this property is actively animated by a keyframe track.
     *
     * Complexity: O(1).
     */
    [[nodiscard]] bool is_animated() const noexcept { return track_storage_.is_animated(); }

    /**
     * @brief Evaluates the property value at the given clip-relative time.
     *
     * Complexity: O(1) if not animated, O(log n) if animated.
     *
     * @param time Clip-relative evaluation time.
     * @return The evaluated value (from the keyframe track if animated, or constant_value()).
     */
    [[nodiscard]] T value_at(ClipTime time) const
        noexcept(std::is_nothrow_copy_constructible_v<T>) {
        static_cast<void>(time);
        if constexpr (keyframes::Animatable<T>) {
            if (track_storage_.track != nullptr) {
                return track_storage_.track->value_at(time.to_core());
            }
        }
        return constant_;
    }

    /**
     * @brief Sets or clears the keyframe track for animatable types.
     *
     * An empty track clears animation (resets shared pointer). Non-empty tracks
     * are stored in a new std::shared_ptr (copy-on-write).
     *
     * @param track The keyframe track to assign.
     */
    template <class U = T>
        requires(std::same_as<U, T> && keyframes::Animatable<U>)
    void set_keyframes(keyframes::KeyframeTrack<U> track) {
        if (track.empty()) {
            track_storage_.track.reset();
        } else {
            track_storage_.track = std::make_shared<keyframes::KeyframeTrack<T>>(std::move(track));
        }
    }

    /**
     * @brief Clears the keyframe track and restores constant evaluation.
     *
     * No-op if not animated.
     */
    template <class U = T>
        requires(std::same_as<U, T> && keyframes::Animatable<U>)
    void clear_keyframes() noexcept {
        track_storage_.track.reset();
    }

    /**
     * @brief Returns a non-owning pointer to the active keyframe track, or nullptr if not animated.
     *
     * Complexity: O(1).
     */
    template <class U = T>
        requires(std::same_as<U, T> && keyframes::Animatable<U>)
    [[nodiscard]] const keyframes::KeyframeTrack<U>* keyframes() const noexcept {
        return track_storage_.track.get();
    }

    /**
     * @brief Evaluates the property value at the given clip-relative time using a playback cursor.
     *
     * Results are bit-identical to value_at(time). If not animated, returns constant_value()
     * and leaves cursor untouched.
     *
     * Complexity: O(1) if not animated, O(1) amortized for sequential playback, O(log n) on jumps.
     *
     * @param time Clip-relative evaluation time.
     * @param cursor Playback cursor caching current segment index.
     * @return The evaluated value.
     */
    template <class U = T>
        requires(std::same_as<U, T> && keyframes::Animatable<U>)
    [[nodiscard]] T value_at(ClipTime time,
                             typename keyframes::KeyframeTrack<U>::Cursor& cursor) const noexcept {
        if (track_storage_.track != nullptr) {
            return track_storage_.track->value_at(time.to_core(), cursor);
        }
        return constant_;
    }

private:
    T constant_{};
    [[no_unique_address]] detail::PropertyTrackStorage<T> track_storage_{};
};

}  // namespace nxtcut::model
