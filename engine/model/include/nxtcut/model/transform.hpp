#pragma once

#include <nxtcut/model/property.hpp>

namespace nxtcut::model {

/**
 * @brief Visual transform and cropping properties for visual clips.
 *
 * Coordinate and unit conventions:
 * - position_x, position_y: Translation in pixels, relative to the canvas center (positive x is
 * right, positive y is down).
 * - scale_x, scale_y: Uniform or non-uniform scaling multipliers (1.0 = 100% native size).
 * - rotation_degrees: Clockwise rotation in degrees around the anchor point.
 * - anchor_x, anchor_y: Pivot offset in pixels, measured relative to the clip center.
 * - opacity: Linear alpha multiplier, ranging from 0.0 (fully transparent) to 1.0 (fully opaque).
 * - crop_left, crop_right, crop_top, crop_bottom: Cropping margins expressed as normalized
 * fractions of native clip dimensions [0.0, 1.0].
 *
 * @note Thread safety: plain data, no internal synchronization; concurrent const access is safe;
 * copies are independent.
 */
struct TransformProps {
    Property<double> position_x{0.0};
    Property<double> position_y{0.0};
    Property<double> scale_x{1.0};
    Property<double> scale_y{1.0};
    Property<double> rotation_degrees{0.0};
    Property<double> anchor_x{0.0};
    Property<double> anchor_y{0.0};
    Property<double> opacity{1.0};
    Property<double> crop_left{0.0};
    Property<double> crop_right{0.0};
    Property<double> crop_top{0.0};
    Property<double> crop_bottom{0.0};
};

}  // namespace nxtcut::model
