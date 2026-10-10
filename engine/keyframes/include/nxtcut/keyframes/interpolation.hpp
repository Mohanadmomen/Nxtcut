#pragma once

#include <nxtcut/core/error.hpp>
#include <nxtcut/core/result.hpp>
#include <nxtcut/keyframes/easing.hpp>

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace nxtcut::keyframes {

/**
 * @brief Keyframe segment interpolation style.
 */
enum class InterpolationKind : std::uint8_t {
    Hold,
    Linear,
    Easing,
    Bezier,
};

/**
 * @brief 2D control point handles for CSS cubic-bezier curves (P1 and P2).
 * P0 is implicitly (0, 0) and P3 is implicitly (1, 1).
 */
struct BezierHandles {
    double x1{0.0};
    double y1{0.0};
    double x2{0.0};
    double y2{0.0};

    [[nodiscard]] constexpr double operator[](std::size_t index) const noexcept {
        if (index == 0)
            return x1;
        if (index == 1)
            return y1;
        if (index == 2)
            return x2;
        return y2;
    }
};

/**
 * @brief Value type describing how progress maps between consecutive keyframes.
 *
 * Supports Hold, Linear, 19 preset Easing curves, and arbitrary CSS cubic-bezier curves.
 * Evaluation on hot paths (Linear, Hold) is completely inlined and allocation-free.
 *
 * @note Thread safety: Thread-safe (immutable trivially-copyable value type).
 */
class Interpolation {
public:
    using Handles = BezierHandles;

    constexpr Interpolation() noexcept = default;

    /**
     * @brief Creates a step (hold) interpolation: progress stays 0.0 until t reaches 1.0.
     */
    [[nodiscard]] static constexpr Interpolation hold() noexcept {
        Interpolation result;
        result.kind_ = InterpolationKind::Hold;
        return result;
    }

    /**
     * @brief Creates a linear interpolation: progress maps identity t.
     */
    [[nodiscard]] static constexpr Interpolation linear() noexcept {
        Interpolation result;
        result.kind_ = InterpolationKind::Linear;
        return result;
    }

    /**
     * @brief Creates an easing preset interpolation.
     */
    [[nodiscard]] static constexpr Interpolation easing(EasingKind kind) noexcept {
        Interpolation result;
        result.kind_ = InterpolationKind::Easing;
        result.easing_kind_ = kind;
        return result;
    }

    /**
     * @brief Creates a CSS cubic-bezier interpolation curve.
     *
     * Precomputes polynomial coefficients for Horner-form evaluation.
     *
     * @param x1 Control point 1 X coordinate in [0.0, 1.0].
     * @param y1 Control point 1 Y coordinate (any finite value).
     * @param x2 Control point 2 X coordinate in [0.0, 1.0].
     * @param y2 Control point 2 Y coordinate (any finite value).
     * @return Interpolation or InvalidArgument if parameters are invalid.
     */
    [[nodiscard]] static core::Result<Interpolation> bezier(double x1, double y1, double x2,
                                                            double y2);

    [[nodiscard]] constexpr InterpolationKind kind() const noexcept { return kind_; }
    [[nodiscard]] constexpr EasingKind easing_kind() const noexcept { return easing_kind_; }
    [[nodiscard]] constexpr BezierHandles handles() const noexcept { return handles_; }

    /**
     * @brief Maps normalized segment progress t into eased progress.
     *
     * Hold and Linear are evaluated directly inline.
     *
     * @param t Segment progress, nominally in [0.0, 1.0].
     * @return Transformed progress.
     * @note Complexity: O(1).
     */
    [[nodiscard]] double map_progress(double t) const noexcept {
        if (kind_ == InterpolationKind::Linear) {
            return t;
        }
        if (kind_ == InterpolationKind::Hold) {
            return (t < 1.0) ? 0.0 : 1.0;
        }
        return map_progress_slow(t);
    }

private:
    [[nodiscard]] double map_progress_slow(double t) const noexcept;

    InterpolationKind kind_{InterpolationKind::Linear};
    EasingKind easing_kind_{EasingKind::Linear};
    BezierHandles handles_{};
    double ax_{0.0};
    double bx_{0.0};
    double cx_{0.0};
    double ay_{0.0};
    double by_{0.0};
    double cy_{0.0};
};

static_assert(std::is_trivially_copyable_v<Interpolation>);

/**
 * @brief Compares two Interpolation instances bit-exactly.
 *
 * Compares kind, easing_kind, and control point handles bit-for-bit.
 */
[[nodiscard]] bool identical(const Interpolation& a, const Interpolation& b) noexcept;

}  // namespace nxtcut::keyframes
