#pragma once

#include <nxtcut/core/result.hpp>
#include <nxtcut/core/time.hpp>
#include <nxtcut/model/clip.hpp>
#include <nxtcut/model/property.hpp>

#include <cstdint>
#include <type_traits>
#include <utility>
#include <variant>

namespace nxtcut::model {

namespace detail {

template <class ClipT, class Fn>
void for_each_animatable_property_impl(ClipT& clip, Fn&& fn) {
    // 1. TransformProps (12 fields in declaration order, always for every clip kind)
    fn(clip.transform.position_x);
    fn(clip.transform.position_y);
    fn(clip.transform.scale_x);
    fn(clip.transform.scale_y);
    fn(clip.transform.rotation_degrees);
    fn(clip.transform.anchor_x);
    fn(clip.transform.anchor_y);
    fn(clip.transform.opacity);
    fn(clip.transform.crop_left);
    fn(clip.transform.crop_right);
    fn(clip.transform.crop_top);
    fn(clip.transform.crop_bottom);

    // 2. AudioContent::volume (if audio)
    if (auto* audio = std::get_if<AudioContent>(&clip.content)) {
        fn(audio->volume);
    }

    // 3. TextContent::font_size_px then ::color (if text)
    if (auto* text = std::get_if<TextContent>(&clip.content)) {
        fn(text->font_size_px);
        fn(text->color);
    }

    // 4. Effects (in clip.effects order, std::map order for params)
    for (auto& effect : clip.effects) {
        for (auto& [param_name, param_val] : effect.params) {
            static_cast<void>(param_name);
            if (auto* d = std::get_if<Property<double>>(&param_val)) {
                fn(*d);
            } else if (auto* c = std::get_if<Property<core::Color>>(&param_val)) {
                fn(*c);
            }
        }
    }
}

}  // namespace detail

/**
 * @brief Visits every animatable property of a clip in a fixed deterministic order.
 *
 * Traversal order:
 * 1. The 12 TransformProps fields in declaration order (position_x, position_y, scale_x, scale_y,
 *    rotation_degrees, anchor_x, anchor_y, opacity, crop_left, crop_right, crop_top, crop_bottom).
 * 2. AudioContent::volume (if audio clip).
 * 3. TextContent::font_size_px, then TextContent::color (if text clip).
 * 4. For each EffectInstance in clip.effects order: each parameter in std::map order whose
 *    variant alternative is Property<double> or Property<core::Color> (skipping bool and string).
 *
 * @tparam Fn Callable accepting Property<double>& or Property<core::Color>&.
 * @param clip The clip whose animatable properties are to be visited.
 * @param fn Visitor callback.
 */
template <class Fn>
void for_each_animatable_property(Clip& clip, Fn&& fn) {
    detail::for_each_animatable_property_impl(clip, std::forward<Fn>(fn));
}

/**
 * @brief Visits every animatable property of a const clip in a fixed deterministic order.
 *
 * @tparam Fn Callable accepting const Property<double>& or const Property<core::Color>&.
 * @param clip The clip whose animatable properties are to be visited.
 * @param fn Visitor callback.
 */
template <class Fn>
void for_each_animatable_property(const Clip& clip, Fn&& fn) {
    detail::for_each_animatable_property_impl(clip, std::forward<Fn>(fn));
}

/**
 * @brief Shifts all keyframe times on every animated property of a clip by an offset.
 *
 * For every animated property: new key time = old time + offset.
 * Resulting timestamps must stay within [-keyframes::kMaxKeyframeTicks,
 * +keyframes::kMaxKeyframeTicks]. Values and interpolation styles are preserved. Non-animated
 * properties are not touched. An offset of 0 is a no-op that preserves track pointers without
 * copying.
 *
 * Complexity:
 * - Non-animated clip: O(P) pointer checks and ZERO allocations (P = number of visited properties).
 * - Animated clip: O(P + K), where K = total keyframes across all animated properties.
 *
 * Error contract:
 * On error, the clip may be partially updated, but every Property still holds a valid track;
 * callers work on scratch copies and discard them on error.
 *
 * @param clip The clip to modify.
 * @param offset Signed duration offset to add to each keyframe time.
 * @return Success, or ErrorCode::InvalidArgument if arithmetic overflows or exceeds tick bounds.
 * @note Thread safety: Mutates the provided clip; caller must ensure exclusive access.
 */
[[nodiscard]] core::Status shift_keyframes(Clip& clip, core::Duration offset);

/**
 * @brief Scales all keyframe times on every animated property of a clip by a rational ratio.
 *
 * For every animated property:
 * new time = core::mul_div(time, numerator, denominator, core::RoundingMode::Nearest).
 * Values and interpolation styles are preserved.
 *
 * Preconditions & Invariants:
 * - Numerator and denominator must be strictly positive (> 0); otherwise returns InvalidArgument.
 * - Equal numerator and denominator is a no-op without copying (track pointers unchanged).
 * - If two keys land on the same tick, returns ErrorCode::InvalidArgument with a message
 *   containing "keyframe collision".
 * - If arithmetic overflows or leaves [-kMaxKeyframeTicks, +kMaxKeyframeTicks], returns an error.
 *
 * Complexity:
 * - Non-animated clip: O(P) pointer checks and ZERO allocations (P = number of visited properties).
 * - Animated clip: O(P + K), where K = total keyframes across all animated properties.
 *
 * Error contract:
 * On error, the clip may be partially updated, but every Property still holds a valid track;
 * callers work on scratch copies and discard them on error.
 *
 * @param clip The clip to modify.
 * @param numerator Positive scaling numerator.
 * @param denominator Positive scaling denominator.
 * @return Success, or ErrorCode::InvalidArgument on collision/range violation, or
 * ErrorCode::Overflow.
 * @note Thread safety: Mutates the provided clip; caller must ensure exclusive access.
 */
[[nodiscard]] core::Status scale_keyframes(Clip& clip, std::int64_t numerator,
                                           std::int64_t denominator);

}  // namespace nxtcut::model
