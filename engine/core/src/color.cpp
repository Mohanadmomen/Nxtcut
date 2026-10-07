#include <nxtcut/core/color.hpp>

#include <algorithm>
#include <cmath>
#include <fmt/format.h>

namespace nxtcut::core {
namespace {

int parse_hex_digit(char c) noexcept {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

}  // namespace

Result<Color> Color::from_hex(std::string_view hex) {
    if (hex.empty() || hex.front() != '#') {
        return make_error(ErrorCode::InvalidArgument, "hex color must begin with '#'");
    }

    const std::string_view digits = hex.substr(1);
    for (char c : digits) {
        if (parse_hex_digit(c) < 0) {
            return make_error(ErrorCode::InvalidArgument, "invalid hex digit");
        }
    }

    if (digits.size() == 3) {
        const int r_val = parse_hex_digit(digits[0]) * 17;
        const int g_val = parse_hex_digit(digits[1]) * 17;
        const int b_val = parse_hex_digit(digits[2]) * 17;
        return Color::from_rgba8(static_cast<std::uint8_t>(r_val),
                                 static_cast<std::uint8_t>(g_val),
                                 static_cast<std::uint8_t>(b_val),
                                 255);
    }

    if (digits.size() == 4) {
        const int r_val = parse_hex_digit(digits[0]) * 17;
        const int g_val = parse_hex_digit(digits[1]) * 17;
        const int b_val = parse_hex_digit(digits[2]) * 17;
        const int a_val = parse_hex_digit(digits[3]) * 17;
        return Color::from_rgba8(static_cast<std::uint8_t>(r_val),
                                 static_cast<std::uint8_t>(g_val),
                                 static_cast<std::uint8_t>(b_val),
                                 static_cast<std::uint8_t>(a_val));
    }

    if (digits.size() == 6) {
        const int r_val = parse_hex_digit(digits[0]) * 16 + parse_hex_digit(digits[1]);
        const int g_val = parse_hex_digit(digits[2]) * 16 + parse_hex_digit(digits[3]);
        const int b_val = parse_hex_digit(digits[4]) * 16 + parse_hex_digit(digits[5]);
        return Color::from_rgba8(static_cast<std::uint8_t>(r_val),
                                 static_cast<std::uint8_t>(g_val),
                                 static_cast<std::uint8_t>(b_val),
                                 255);
    }

    if (digits.size() == 8) {
        const int r_val = parse_hex_digit(digits[0]) * 16 + parse_hex_digit(digits[1]);
        const int g_val = parse_hex_digit(digits[2]) * 16 + parse_hex_digit(digits[3]);
        const int b_val = parse_hex_digit(digits[4]) * 16 + parse_hex_digit(digits[5]);
        const int a_val = parse_hex_digit(digits[6]) * 16 + parse_hex_digit(digits[7]);
        return Color::from_rgba8(static_cast<std::uint8_t>(r_val),
                                 static_cast<std::uint8_t>(g_val),
                                 static_cast<std::uint8_t>(b_val),
                                 static_cast<std::uint8_t>(a_val));
    }

    return make_error(ErrorCode::InvalidArgument, "unsupported hex color length");
}

std::array<std::uint8_t, 4> Color::to_rgba8() const noexcept {
    auto convert = [](float v) noexcept -> std::uint8_t {
        const float clamped = std::clamp(v, 0.0f, 1.0f);
        return static_cast<std::uint8_t>(std::lround(clamped * 255.0f));
    };
    return {convert(r), convert(g), convert(b), convert(a)};
}

std::string Color::to_hex(bool include_alpha) const {
    const auto rgba = to_rgba8();
    if (include_alpha) {
        return fmt::format("#{:02x}{:02x}{:02x}{:02x}", rgba[0], rgba[1], rgba[2], rgba[3]);
    }
    return fmt::format("#{:02x}{:02x}{:02x}", rgba[0], rgba[1], rgba[2]);
}

Color Color::to_linear() const noexcept {
    return Color{srgb_to_linear(r), srgb_to_linear(g), srgb_to_linear(b), a};
}

Color Color::to_srgb() const noexcept {
    return Color{linear_to_srgb(r), linear_to_srgb(g), linear_to_srgb(b), a};
}

float srgb_to_linear(float c) noexcept {
    const float clamped = std::clamp(c, 0.0f, 1.0f);
    if (clamped <= 0.04045f) {
        return clamped / 12.92f;
    }
    return std::pow((clamped + 0.055f) / 1.055f, 2.4f);
}

float linear_to_srgb(float c) noexcept {
    const float clamped = std::clamp(c, 0.0f, 1.0f);
    if (clamped <= 0.0031308f) {
        return 12.92f * clamped;
    }
    return 1.055f * std::pow(clamped, 1.0f / 2.4f) - 0.055f;
}

}  // namespace nxtcut::core
