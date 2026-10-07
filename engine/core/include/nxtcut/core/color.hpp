#pragma once

#include <nxtcut/core/result.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>

namespace nxtcut::core {

struct PremultipliedColor;

/**
 * @brief Straight (non-premultiplied) RGBA color with float components.
 *
 * Components are nominally in [0.0, 1.0] and are not clamped upon construction.
 * The struct stores whatever color space encoding the caller provides (linear or sRGB-encoded).
 * Transfer function helpers (to_linear(), to_srgb()) convert explicitly between encodings.
 *
 * @note Thread safety: Thread-safe (immutable value semantics).
 */
struct Color {
    float r{0.0f};
    float g{0.0f};
    float b{0.0f};
    float a{1.0f};

    /**
     * @brief Constructs a Color from 8-bit unsigned integer channels.
     */
    [[nodiscard]] static constexpr Color from_rgba8(
        std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255) noexcept {
        return Color{
            static_cast<float>(r) / 255.0f,
            static_cast<float>(g) / 255.0f,
            static_cast<float>(b) / 255.0f,
            static_cast<float>(a) / 255.0f,
        };
    }

    /**
     * @brief Parses a hex color string (#RGB, #RGBA, #RRGGBB, #RRGGBBAA).
     *
     * Short forms expand each nibble n to n * 17.
     *
     * @param hex Hex string starting with '#'.
     * @return Color or ErrorCode::InvalidArgument if invalid.
     */
    [[nodiscard]] static Result<Color> from_hex(std::string_view hex);

    /**
     * @brief Converts color channels to 8-bit unsigned integers [0, 255] (rounded to nearest, clamped).
     */
    [[nodiscard]] std::array<std::uint8_t, 4> to_rgba8() const noexcept;

    /**
     * @brief Formats this color as a lowercase hex string (e.g. #336699 or #336699cc).
     *
     * @param include_alpha If true, formats 8 hex digits; otherwise 6 hex digits.
     */
    [[nodiscard]] std::string to_hex(bool include_alpha = true) const;

    /**
     * @brief Converts this straight color to premultiplied alpha form (r*a, g*a, b*a, a).
     */
    [[nodiscard]] PremultipliedColor premultiplied() const noexcept;

    /**
     * @brief Converts RGB channels from sRGB transfer characteristics to linear space. Alpha unchanged.
     */
    [[nodiscard]] Color to_linear() const noexcept;

    /**
     * @brief Converts RGB channels from linear space to sRGB transfer characteristics. Alpha unchanged.
     */
    [[nodiscard]] Color to_srgb() const noexcept;

    /**
     * @brief Compares components for approximate equality within tolerance.
     */
    [[nodiscard]] bool approx_equal(const Color& other, float eps = 1e-6f) const noexcept {
        return std::abs(r - other.r) <= eps &&
               std::abs(g - other.g) <= eps &&
               std::abs(b - other.b) <= eps &&
               std::abs(a - other.a) <= eps;
    }
};

/**
 * @brief Premultiplied RGBA color (r * a, g * a, b * a, a).
 *
 * Distinct type to prevent accidental mixing of straight and premultiplied colors.
 *
 * @note Thread safety: Thread-safe (immutable value semantics).
 */
struct PremultipliedColor {
    float r{0.0f};
    float g{0.0f};
    float b{0.0f};
    float a{1.0f};

    /**
     * @brief Un-premultiplies color channels (r/a, g/a, b/a, a).
     *
     * If alpha is 0, returns (0, 0, 0, 0) without division by zero.
     */
    [[nodiscard]] Color unpremultiplied() const noexcept;

    /**
     * @brief Compares components for approximate equality within tolerance.
     */
    [[nodiscard]] bool approx_equal(const PremultipliedColor& other, float eps = 1e-6f) const noexcept {
        return std::abs(r - other.r) <= eps &&
               std::abs(g - other.g) <= eps &&
               std::abs(b - other.b) <= eps &&
               std::abs(a - other.a) <= eps;
    }
};

inline PremultipliedColor Color::premultiplied() const noexcept {
    return PremultipliedColor{r * a, g * a, b * a, a};
}

inline Color PremultipliedColor::unpremultiplied() const noexcept {
    if (a == 0.0f) {
        return Color{0.0f, 0.0f, 0.0f, 0.0f};
    }
    return Color{r / a, g / a, b / a, a};
}

/**
 * @brief Component-wise linear interpolation between straight colors.
 *
 * Parameter t is not clamped (extrapolation allowed). Returns exact endpoints at t = 0 and t = 1.
 * Note: Blending colors with differing alpha should be performed on PremultipliedColor.
 */
[[nodiscard]] inline Color lerp(const Color& c1, const Color& c2, float t) noexcept {
    if (t == 0.0f) {
        return c1;
    }
    if (t == 1.0f) {
        return c2;
    }
    return Color{
        c1.r + t * (c2.r - c1.r),
        c1.g + t * (c2.g - c1.g),
        c1.b + t * (c2.b - c1.b),
        c1.a + t * (c2.a - c1.a),
    };
}

/**
 * @brief Component-wise linear interpolation between premultiplied colors.
 *
 * Parameter t is not clamped (extrapolation allowed). Returns exact endpoints at t = 0 and t = 1.
 */
[[nodiscard]] inline PremultipliedColor lerp(
    const PremultipliedColor& c1, const PremultipliedColor& c2, float t) noexcept {
    if (t == 0.0f) {
        return c1;
    }
    if (t == 1.0f) {
        return c2;
    }
    return PremultipliedColor{
        c1.r + t * (c2.r - c1.r),
        c1.g + t * (c2.g - c1.g),
        c1.b + t * (c2.b - c1.b),
        c1.a + t * (c2.a - c1.a),
    };
}

/**
 * @brief Converts a single sRGB transfer-encoded component to linear space (IEC 61966-2-1).
 *
 * Clamps input to [0.0, 1.0] before applying conversion.
 */
[[nodiscard]] float srgb_to_linear(float c) noexcept;

/**
 * @brief Converts a single linear component to sRGB transfer characteristics (IEC 61966-2-1).
 *
 * Clamps input to [0.0, 1.0] before applying conversion.
 */
[[nodiscard]] float linear_to_srgb(float c) noexcept;

}  // namespace nxtcut::core
