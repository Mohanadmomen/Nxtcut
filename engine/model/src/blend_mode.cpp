#include <nxtcut/model/blend_mode.hpp>

namespace nxtcut::model {

std::string_view to_string(BlendMode mode) noexcept {
    switch (mode) {
        case BlendMode::Normal:
            return "Normal";
        case BlendMode::Multiply:
            return "Multiply";
        case BlendMode::Screen:
            return "Screen";
        case BlendMode::Overlay:
            return "Overlay";
        case BlendMode::Darken:
            return "Darken";
        case BlendMode::Lighten:
            return "Lighten";
        case BlendMode::ColorDodge:
            return "ColorDodge";
        case BlendMode::ColorBurn:
            return "ColorBurn";
        case BlendMode::HardLight:
            return "HardLight";
        case BlendMode::SoftLight:
            return "SoftLight";
        case BlendMode::Difference:
            return "Difference";
        case BlendMode::Exclusion:
            return "Exclusion";
        case BlendMode::Add:
            return "Add";
    }
    return "Unknown";
}

}  // namespace nxtcut::model
