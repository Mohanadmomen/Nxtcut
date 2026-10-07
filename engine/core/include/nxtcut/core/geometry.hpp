#pragma once

#include <nxtcut/core/result.hpp>

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <optional>
#include <type_traits>

namespace nxtcut::core {

/**
 * @brief Constrains arithmetic types for geometry primitives (integral or floating point, excluding bool).
 */
template <class T>
concept Arithmetic = (std::integral<T> || std::floating_point<T>) &&
                     !std::same_as<std::remove_cv_t<T>, bool>;

/**
 * @brief Represents a 2D position (x, y).
 *
 * @note Thread safety: Thread-safe (immutable value semantics).
 */
template <Arithmetic T>
struct Point {
    T x{};
    T y{};

    constexpr bool operator==(const Point&) const requires std::integral<T> = default;
};

/**
 * @brief Represents 2D extents (width, height).
 *
 * @note Thread safety: Thread-safe (immutable value semantics).
 */
template <Arithmetic T>
struct Size {
    T width{};
    T height{};

    /**
     * @brief Checks whether either dimension is non-positive (width <= 0 or height <= 0).
     */
    [[nodiscard]] constexpr bool is_empty() const noexcept {
        return width <= static_cast<T>(0) || height <= static_cast<T>(0);
    }

    constexpr bool operator==(const Size&) const requires std::integral<T> = default;
};

/**
 * @brief Represents a 2D axis-aligned rectangle (x, y, width, height).
 *
 * Intervals are treated as half-open [x, x + width) horizontally and [y, y + height) vertically.
 *
 * @note Thread safety: Thread-safe (immutable value semantics).
 */
template <Arithmetic T>
struct Rect {
    T x{};
    T y{};
    T width{};
    T height{};

    /**
     * @brief Precondition: x must not overflow when computing right boundary.
     */
    [[nodiscard]] constexpr T left() const noexcept { return x; }

    /**
     * @brief Precondition: y must not overflow when computing bottom boundary.
     */
    [[nodiscard]] constexpr T top() const noexcept { return y; }

    /**
     * @brief Precondition: x + width must not overflow integer bounds.
     */
    [[nodiscard]] constexpr T right() const noexcept { return x + width; }

    /**
     * @brief Precondition: y + height must not overflow integer bounds.
     */
    [[nodiscard]] constexpr T bottom() const noexcept { return y + height; }

    /**
     * @brief Checks whether the rectangle has non-positive area (width <= 0 or height <= 0).
     */
    [[nodiscard]] constexpr bool is_empty() const noexcept {
        return width <= static_cast<T>(0) || height <= static_cast<T>(0);
    }

    /**
     * @brief Half-open containment check: left <= px < right and top <= py < bottom.
     */
    [[nodiscard]] constexpr bool contains(const Point<T>& p) const noexcept {
        if (is_empty()) {
            return false;
        }
        return p.x >= left() && p.x < right() && p.y >= top() && p.y < bottom();
    }

    /**
     * @brief Checks whether other is entirely contained within this rectangle.
     */
    [[nodiscard]] constexpr bool contains(const Rect<T>& other) const noexcept {
        if (is_empty() || other.is_empty()) {
            return false;
        }
        return other.left() >= left() && other.right() <= right() &&
               other.top() >= top() && other.bottom() <= bottom();
    }

    /**
     * @brief Checks whether two rectangles overlap. Touching edges do not intersect.
     */
    [[nodiscard]] constexpr bool intersects(const Rect<T>& other) const noexcept {
        if (is_empty() || other.is_empty()) {
            return false;
        }
        return left() < other.right() && right() > other.left() &&
               top() < other.bottom() && bottom() > other.top();
    }

    /**
     * @brief Returns the overlapping intersection rectangle, or std::nullopt if disjoint.
     */
    [[nodiscard]] constexpr std::optional<Rect<T>> intersection(const Rect<T>& other) const noexcept {
        if (!intersects(other)) {
            return std::nullopt;
        }
        const T x1 = std::max(left(), other.left());
        const T y1 = std::max(top(), other.top());
        const T x2 = std::min(right(), other.right());
        const T y2 = std::min(bottom(), other.bottom());
        return Rect<T>{x1, y1, x2 - x1, y2 - y1};
    }

    /**
     * @brief Returns the bounding union rectangle. Empty rectangles are ignored.
     */
    [[nodiscard]] constexpr Rect<T> united(const Rect<T>& other) const noexcept {
        if (is_empty() && other.is_empty()) {
            return Rect<T>{};
        }
        if (is_empty()) {
            return other;
        }
        if (other.is_empty()) {
            return *this;
        }
        const T x1 = std::min(left(), other.left());
        const T y1 = std::min(top(), other.top());
        const T x2 = std::max(right(), other.right());
        const T y2 = std::max(bottom(), other.bottom());
        return Rect<T>{x1, y1, x2 - x1, y2 - y1};
    }

    /**
     * @brief Center point of the rectangle.
     */
    [[nodiscard]] constexpr Point<T> center() const noexcept {
        return Point<T>{x + width / static_cast<T>(2), y + height / static_cast<T>(2)};
    }

    constexpr bool operator==(const Rect&) const requires std::integral<T> = default;
};

template <Arithmetic T>
[[nodiscard]] constexpr bool approx_equal(const Point<T>& a,
                                          const Point<T>& b,
                                          T epsilon = static_cast<T>(1e-9)) noexcept {
    if constexpr (std::floating_point<T>) {
        return std::abs(a.x - b.x) <= epsilon && std::abs(a.y - b.y) <= epsilon;
    } else {
        return a.x == b.x && a.y == b.y;
    }
}

template <Arithmetic T>
[[nodiscard]] constexpr bool approx_equal(const Size<T>& a,
                                          const Size<T>& b,
                                          T epsilon = static_cast<T>(1e-9)) noexcept {
    if constexpr (std::floating_point<T>) {
        return std::abs(a.width - b.width) <= epsilon && std::abs(a.height - b.height) <= epsilon;
    } else {
        return a.width == b.width && a.height == b.height;
    }
}

template <Arithmetic T>
[[nodiscard]] constexpr bool approx_equal(const Rect<T>& a,
                                          const Rect<T>& b,
                                          T epsilon = static_cast<T>(1e-9)) noexcept {
    if constexpr (std::floating_point<T>) {
        return std::abs(a.x - b.x) <= epsilon &&
               std::abs(a.y - b.y) <= epsilon &&
               std::abs(a.width - b.width) <= epsilon &&
               std::abs(a.height - b.height) <= epsilon;
    } else {
        return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
    }
}

using PointI = Point<std::int32_t>;
using PointD = Point<double>;
using SizeI = Size<std::int32_t>;
using SizeD = Size<double>;
using RectI = Rect<std::int32_t>;
using RectD = Rect<double>;

/**
 * @brief Computes uniformly scaled size fitting inside container while preserving aspect ratio.
 */
[[nodiscard]] inline Result<SizeD> fit_inside(SizeD content, SizeD container) {
    if (!std::isfinite(content.width) || !std::isfinite(content.height) ||
        !std::isfinite(container.width) || !std::isfinite(container.height) ||
        content.width <= 0.0 || content.height <= 0.0 ||
        container.width <= 0.0 || container.height <= 0.0) {
        return make_error(ErrorCode::InvalidArgument, "dimensions must be positive and finite");
    }
    const double scale = std::min(container.width / content.width, container.height / content.height);
    return SizeD{content.width * scale, content.height * scale};
}

/**
 * @brief Computes uniformly scaled size filling outside container while preserving aspect ratio.
 */
[[nodiscard]] inline Result<SizeD> fill_outside(SizeD content, SizeD container) {
    if (!std::isfinite(content.width) || !std::isfinite(content.height) ||
        !std::isfinite(container.width) || !std::isfinite(container.height) ||
        content.width <= 0.0 || content.height <= 0.0 ||
        container.width <= 0.0 || container.height <= 0.0) {
        return make_error(ErrorCode::InvalidArgument, "dimensions must be positive and finite");
    }
    const double scale = std::max(container.width / content.width, container.height / content.height);
    return SizeD{content.width * scale, content.height * scale};
}

/**
 * @brief Computes letterboxed rectangle centered within container.
 */
[[nodiscard]] inline Result<RectD> letterbox_rect(SizeD content, SizeD container) {
    const auto fit_res = fit_inside(content, container);
    if (!fit_res.has_value()) {
        return tl::unexpected(fit_res.error());
    }
    const SizeD fit = fit_res.value();
    const double x = (container.width - fit.width) / 2.0;
    const double y = (container.height - fit.height) / 2.0;
    return RectD{x, y, fit.width, fit.height};
}

}  // namespace nxtcut::core
