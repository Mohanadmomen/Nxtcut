#pragma once

#include <cstdint>
#include <string_view>

namespace nxtcut::model {

/**
 * @brief Blend modes available for visual compositing.
 */
enum class BlendMode : std::uint8_t {
    Normal,
    Multiply,
    Screen,
    Overlay,
    Darken,
    Lighten,
    ColorDodge,
    ColorBurn,
    HardLight,
    SoftLight,
    Difference,
    Exclusion,
    Add,
};

/**
 * @brief Converts a BlendMode enumerator to its string name.
 */
[[nodiscard]] std::string_view to_string(BlendMode mode) noexcept;

}  // namespace nxtcut::model
