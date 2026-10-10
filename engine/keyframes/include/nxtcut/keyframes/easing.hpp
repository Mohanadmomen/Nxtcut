#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace nxtcut::keyframes {

/**
 * @brief Easing curve families and direction variants.
 *
 * Exactly 19 values: Linear plus EaseIn, EaseOut, EaseInOut for
 * Sine, Quad, Cubic, Quart, Expo, and Circ.
 */
enum class EasingKind : std::uint8_t {
    Linear,
    EaseInSine,
    EaseOutSine,
    EaseInOutSine,
    EaseInQuad,
    EaseOutQuad,
    EaseInOutQuad,
    EaseInCubic,
    EaseOutCubic,
    EaseInOutCubic,
    EaseInQuart,
    EaseOutQuart,
    EaseInOutQuart,
    EaseInExpo,
    EaseOutExpo,
    EaseInOutExpo,
    EaseInCirc,
    EaseOutCirc,
    EaseInOutCirc,
};

/**
 * @brief Evaluates an easing curve for progress t in [0.0, 1.0].
 *
 * Input t is clamped to [0.0, 1.0]; NaN is treated as 0.0.
 * Endpoints are exact: ease(kind, 0.0) == 0.0 and ease(kind, 1.0) == 1.0 for all kinds.
 *
 * @param kind Easing curve variant.
 * @param t Progress value.
 * @return Eased progress in [0.0, 1.0].
 * @note Thread safety: Thread-safe (re-entrant pure function).
 * @note Complexity: O(1).
 */
[[nodiscard]] double ease(EasingKind kind, double t) noexcept;

/**
 * @brief Returns canonical string representation of an EasingKind.
 *
 * @param kind The easing kind enum value.
 * @return String view (e.g. "linear", "ease-in-quad").
 */
[[nodiscard]] std::string_view to_string(EasingKind kind) noexcept;

/**
 * @brief Parses an EasingKind from its canonical string representation.
 *
 * @param str The string view to parse.
 * @return EasingKind or nullopt if unknown.
 */
[[nodiscard]] std::optional<EasingKind> easing_from_string(std::string_view str) noexcept;

}  // namespace nxtcut::keyframes
