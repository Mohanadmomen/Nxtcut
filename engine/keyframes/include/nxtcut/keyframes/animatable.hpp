#pragma once

#include <nxtcut/core/color.hpp>
#include <nxtcut/core/geometry.hpp>

#include <bit>
#include <cmath>
#include <concepts>
#include <cstdint>

namespace nxtcut::keyframes {

/**
 * @brief Extension point trait defining animation behavior for animatable types.
 *
 * Primary template is deliberately left undefined.
 */
template <class T>
struct AnimatableTraits;

/**
 * @brief Concept constraining types that can be animated with KeyframeTrack.
 */
template <class T>
concept Animatable = requires(const T& a, const T& b, double t) {
    { AnimatableTraits<T>::interpolate(a, b, t) } noexcept -> std::same_as<T>;
    { AnimatableTraits<T>::is_finite(a) } noexcept -> std::same_as<bool>;
    { AnimatableTraits<T>::identical(a, b) } noexcept -> std::same_as<bool>;
    { AnimatableTraits<T>::default_value() } noexcept -> std::same_as<T>;
};

// ============================================================================
// Specialization: double
// ============================================================================

template <>
struct AnimatableTraits<double> {
    [[nodiscard]] static constexpr double interpolate(double a, double b, double t) noexcept {
        if (t == 0.0) {
            return a;
        }
        if (t == 1.0) {
            return b;
        }
        return a + t * (b - a);
    }

    [[nodiscard]] static bool is_finite(double v) noexcept { return std::isfinite(v); }

    [[nodiscard]] static bool identical(double a, double b) noexcept {
        return std::bit_cast<std::uint64_t>(a) == std::bit_cast<std::uint64_t>(b);
    }

    [[nodiscard]] static constexpr double default_value() noexcept { return 0.0; }
};

// ============================================================================
// Specialization: core::Color
// ============================================================================

template <>
struct AnimatableTraits<core::Color> {
    /**
     * @brief Interpolates between two straight colors using core::lerp.
     *
     * Note: Parameter t is passed as float to core::lerp. core::lerp does NOT clamp t to [0, 1]
     * or clamp resulting color channel values to [0.0f, 1.0f]; extrapolation and bezier overshoot
     * are permitted. Downstream consumers (e.g. renderers) clamp channels if needed. Returns
     * exact endpoints at t = 0.0 and t = 1.0.
     */
    [[nodiscard]] static core::Color interpolate(const core::Color& a, const core::Color& b,
                                                 double t) noexcept {
        if (t == 0.0) {
            return a;
        }
        if (t == 1.0) {
            return b;
        }
        return core::lerp(a, b, static_cast<float>(t));
    }

    [[nodiscard]] static bool is_finite(const core::Color& c) noexcept {
        return std::isfinite(c.r) && std::isfinite(c.g) && std::isfinite(c.b) && std::isfinite(c.a);
    }

    [[nodiscard]] static bool identical(const core::Color& a, const core::Color& b) noexcept {
        return std::bit_cast<std::uint32_t>(a.r) == std::bit_cast<std::uint32_t>(b.r) &&
               std::bit_cast<std::uint32_t>(a.g) == std::bit_cast<std::uint32_t>(b.g) &&
               std::bit_cast<std::uint32_t>(a.b) == std::bit_cast<std::uint32_t>(b.b) &&
               std::bit_cast<std::uint32_t>(a.a) == std::bit_cast<std::uint32_t>(b.a);
    }

    [[nodiscard]] static constexpr core::Color default_value() noexcept { return core::Color{}; }
};

// ============================================================================
// Specialization: core::Point<double>
// ============================================================================

template <>
struct AnimatableTraits<core::Point<double>> {
    [[nodiscard]] static core::Point<double> interpolate(const core::Point<double>& a,
                                                         const core::Point<double>& b,
                                                         double t) noexcept {
        if (t == 0.0) {
            return a;
        }
        if (t == 1.0) {
            return b;
        }
        return core::Point<double>{a.x + t * (b.x - a.x), a.y + t * (b.y - a.y)};
    }

    [[nodiscard]] static bool is_finite(const core::Point<double>& p) noexcept {
        return std::isfinite(p.x) && std::isfinite(p.y);
    }

    [[nodiscard]] static bool identical(const core::Point<double>& a,
                                        const core::Point<double>& b) noexcept {
        return std::bit_cast<std::uint64_t>(a.x) == std::bit_cast<std::uint64_t>(b.x) &&
               std::bit_cast<std::uint64_t>(a.y) == std::bit_cast<std::uint64_t>(b.y);
    }

    [[nodiscard]] static constexpr core::Point<double> default_value() noexcept {
        return core::Point<double>{0.0, 0.0};
    }
};

}  // namespace nxtcut::keyframes
