#pragma once

#include <nxtcut/core/time.hpp>
#include <nxtcut/keyframes/animatable.hpp>
#include <nxtcut/keyframes/interpolation.hpp>

namespace nxtcut::keyframes {

/**
 * @brief Represents a value keyframe on a timeline track.
 *
 * @tparam T The animatable payload type.
 * @note Time is relative to the owner's origin.
 * @note Interpolation describes the transition to the NEXT keyframe; it is ignored on the last
 * keyframe.
 */
template <Animatable T>
struct Keyframe {
    core::TimePoint time{};
    T value{};
    Interpolation interpolation{};
};

/**
 * @brief Tests two Keyframe instances for exact bit-level equality.
 */
template <Animatable T>
[[nodiscard]] bool identical(const Keyframe<T>& a, const Keyframe<T>& b) noexcept {
    return a.time == b.time && AnimatableTraits<T>::identical(a.value, b.value) &&
           identical(a.interpolation, b.interpolation);
}

}  // namespace nxtcut::keyframes
