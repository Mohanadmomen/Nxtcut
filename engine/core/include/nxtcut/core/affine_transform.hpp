#pragma once

#include <nxtcut/core/geometry.hpp>
#include <nxtcut/core/result.hpp>

namespace nxtcut::core {

/**
 * @brief 2D affine transformation represented by a 3x3 homogeneous matrix.
 *
 * Follows column-vector convention:
 *   [x']   [a  c  tx] [x]
 *   [y'] = [b  d  ty] [y]
 *   [1 ]   [0  0  1 ] [1]
 *
 * Mapping formulas:
 *   x' = a * x + c * y + tx
 *   y' = b * x + d * y + ty
 *
 * @note Thread safety: Thread-safe (immutable value type).
 */
class AffineTransform {
public:
    constexpr AffineTransform() noexcept = default;

    /**
     * @brief Creates the identity transform.
     */
    [[nodiscard]] static constexpr AffineTransform identity() noexcept {
        return AffineTransform(1.0, 0.0, 0.0, 1.0, 0.0, 0.0);
    }

    /**
     * @brief Creates a translation transform.
     */
    [[nodiscard]] static constexpr AffineTransform translation(double tx, double ty) noexcept {
        return AffineTransform(1.0, 0.0, 0.0, 1.0, tx, ty);
    }

    /**
     * @brief Creates a scale transform.
     */
    [[nodiscard]] static constexpr AffineTransform scale(double sx, double sy) noexcept {
        return AffineTransform(sx, 0.0, 0.0, sy, 0.0, 0.0);
    }

    /**
     * @brief Creates a rotation transform by radians.
     *
     * Positive angles rotate the x axis towards the y axis: (x, y) -> (x cos - y sin, x sin + y
     * cos). On screens where y points downwards, this corresponds to clockwise rotation.
     */
    [[nodiscard]] static AffineTransform rotation(double radians) noexcept;

    /**
     * @brief Creates an affine transform with raw matrix components.
     */
    [[nodiscard]] static constexpr AffineTransform from_matrix(double a, double b, double c,
                                                               double d, double tx,
                                                               double ty) noexcept {
        return AffineTransform(a, b, c, d, tx, ty);
    }

    /**
     * @brief Composes transformations such that this transform is applied FIRST, and next is
     * applied SECOND.
     *
     * Guarantees: this->then(next).apply(p) == next.apply(this->apply(p)).
     */
    [[nodiscard]] AffineTransform then(const AffineTransform& next) const noexcept;

    /**
     * @brief Transforms a 2D point.
     */
    [[nodiscard]] PointD apply(PointD p) const noexcept;

    /**
     * @brief Transforms a rectangle and returns the axis-aligned bounding box of the four
     * transformed corners.
     */
    [[nodiscard]] RectD apply(RectD rect) const noexcept;

    /**
     * @brief Computes the matrix determinant (a * d - b * c).
     */
    [[nodiscard]] constexpr double determinant() const noexcept { return a_ * d_ - b_ * c_; }

    /**
     * @brief Computes the inverse transformation.
     *
     * @return Inverse AffineTransform, or ErrorCode::InvalidArgument if determinant magnitude <=
     * 1e-12 or elements are non-finite.
     */
    [[nodiscard]] Result<AffineTransform> inverse() const;

    [[nodiscard]] constexpr double a() const noexcept { return a_; }
    [[nodiscard]] constexpr double b() const noexcept { return b_; }
    [[nodiscard]] constexpr double c() const noexcept { return c_; }
    [[nodiscard]] constexpr double d() const noexcept { return d_; }
    [[nodiscard]] constexpr double tx() const noexcept { return tx_; }
    [[nodiscard]] constexpr double ty() const noexcept { return ty_; }

    /**
     * @brief Compares components for approximate equality within tolerance.
     */
    [[nodiscard]] bool approx_equal(const AffineTransform& other,
                                    double epsilon = 1e-9) const noexcept;

private:
    constexpr AffineTransform(double a, double b, double c, double d, double tx, double ty) noexcept
        : a_(a), b_(b), c_(c), d_(d), tx_(tx), ty_(ty) {}

    double a_{1.0};
    double b_{0.0};
    double c_{0.0};
    double d_{1.0};
    double tx_{0.0};
    double ty_{0.0};
};

}  // namespace nxtcut::core
