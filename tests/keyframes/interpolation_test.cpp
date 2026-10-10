#include <nxtcut/keyframes/easing.hpp>
#include <nxtcut/keyframes/interpolation.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <initializer_list>
#include <limits>
#include <type_traits>
#include <utility>

#include <nxtcut_test/assertions.hpp>

namespace nxtcut::keyframes {
namespace {

static_assert(std::is_trivially_copyable_v<Interpolation>);
static_assert(noexcept(std::declval<Interpolation>().map_progress(0.0)));
static_assert(noexcept(ease(EasingKind::Linear, 0.0)));

TEST(InterpolationTest, DefaultConstructedIsLinear) {
    const Interpolation interp;
    EXPECT_EQ(interp.kind(), InterpolationKind::Linear);
    EXPECT_EQ(interp.map_progress(0.42), 0.42);
}

TEST(InterpolationTest, BezierValidation) {
    const double nan_val = std::numeric_limits<double>::quiet_NaN();
    const double inf_val = std::numeric_limits<double>::infinity();

    // Rejects NaN and infinity
    EXPECT_TRUE(test::is_error(Interpolation::bezier(nan_val, 0.0, 1.0, 1.0),
                               core::ErrorCode::InvalidArgument));
    EXPECT_TRUE(test::is_error(Interpolation::bezier(0.0, nan_val, 1.0, 1.0),
                               core::ErrorCode::InvalidArgument));
    EXPECT_TRUE(test::is_error(Interpolation::bezier(0.0, 0.0, inf_val, 1.0),
                               core::ErrorCode::InvalidArgument));
    EXPECT_TRUE(test::is_error(Interpolation::bezier(0.0, 0.0, 1.0, -inf_val),
                               core::ErrorCode::InvalidArgument));

    // Rejects x1 or x2 outside [0, 1]
    EXPECT_TRUE(test::is_error(Interpolation::bezier(-0.01, 0.0, 1.0, 1.0),
                               core::ErrorCode::InvalidArgument));
    EXPECT_TRUE(test::is_error(Interpolation::bezier(1.01, 0.0, 1.0, 1.0),
                               core::ErrorCode::InvalidArgument));
    EXPECT_TRUE(test::is_error(Interpolation::bezier(0.0, 0.0, -0.01, 1.0),
                               core::ErrorCode::InvalidArgument));
    EXPECT_TRUE(test::is_error(Interpolation::bezier(0.0, 0.0, 1.01, 1.0),
                               core::ErrorCode::InvalidArgument));

    // Accepts y outside [0, 1] (overshoot)
    EXPECT_TRUE(test::is_ok(Interpolation::bezier(0.2, -0.5, 0.8, 1.5)));
}

TEST(InterpolationTest, BezierLinearEquivalent) {
    const auto res = Interpolation::bezier(1.0 / 3.0, 1.0 / 3.0, 2.0 / 3.0, 2.0 / 3.0);
    ASSERT_TRUE(res.has_value());
    const auto interp = res.value();

    for (const double t : {0.0, 0.1, 0.5, 0.9, 1.0}) {
        EXPECT_NEAR(interp.map_progress(t), t, 1e-9);
    }
}

TEST(InterpolationTest, BezierStandardCurves) {
    // CSS "ease" = (0.25, 0.1, 0.25, 1.0)
    const auto ease_curve = Interpolation::bezier(0.25, 0.1, 0.25, 1.0);
    ASSERT_TRUE(ease_curve.has_value());
    EXPECT_NEAR(ease_curve->map_progress(0.5), 0.8024, 1e-4);

    // Symmetric curve (0.42, 0, 0.58, 1)
    const auto sym_curve = Interpolation::bezier(0.42, 0.0, 0.58, 1.0);
    ASSERT_TRUE(sym_curve.has_value());
    EXPECT_NEAR(sym_curve->map_progress(0.5), 0.5, 1e-9);

    const double t = 0.2;
    EXPECT_NEAR(sym_curve->map_progress(t) + sym_curve->map_progress(1.0 - t), 1.0, 1e-9);
}

TEST(InterpolationTest, EndpointsExact) {
    const Interpolation hold = Interpolation::hold();
    const Interpolation linear = Interpolation::linear();
    const Interpolation easing = Interpolation::easing(EasingKind::EaseInOutCubic);
    const Interpolation bezier = Interpolation::bezier(0.25, 0.1, 0.25, 1.0).value();

    for (const auto& interp : {hold, linear, easing, bezier}) {
        EXPECT_EQ(interp.map_progress(0.0), 0.0);
        EXPECT_EQ(interp.map_progress(1.0), 1.0);
    }

    // Hold step behavior
    EXPECT_EQ(hold.map_progress(-0.1), 0.0);
    EXPECT_EQ(hold.map_progress(0.0), 0.0);
    EXPECT_EQ(hold.map_progress(0.5), 0.0);
    EXPECT_EQ(hold.map_progress(0.99999), 0.0);
    EXPECT_EQ(hold.map_progress(1.0), 1.0);
    EXPECT_EQ(hold.map_progress(1.5), 1.0);
}

TEST(InterpolationTest, BezierOvershoot) {
    const auto curve = Interpolation::bezier(0.3, -0.5, 0.7, 1.5).value();

    bool has_negative = false;
    for (int i = 1; i < 300; ++i) {
        const double t = static_cast<double>(i) / 1000.0;  // in (0, 0.3)
        if (curve.map_progress(t) < 0.0) {
            has_negative = true;
            break;
        }
    }
    EXPECT_TRUE(has_negative);

    bool has_above_one = false;
    for (int i = 701; i < 1000; ++i) {
        const double t = static_cast<double>(i) / 1000.0;  // in (0.7, 1.0)
        if (curve.map_progress(t) > 1.0) {
            has_above_one = true;
            break;
        }
    }
    EXPECT_TRUE(has_above_one);
}

TEST(InterpolationTest, BezierMonotoneWhenHandlesInUnitRange) {
    const auto curve = Interpolation::bezier(0.42, 0.0, 0.58, 1.0).value();
    double prev = 0.0;
    for (int i = 0; i <= 1000; ++i) {
        const double t = static_cast<double>(i) / 1000.0;
        const double val = curve.map_progress(t);
        EXPECT_GE(val, prev);
        prev = val;
    }
}

TEST(InterpolationTest, IdenticalComparison) {
    const Interpolation lin1 = Interpolation::linear();
    const Interpolation lin2 = Interpolation::linear();
    const Interpolation hold = Interpolation::hold();
    EXPECT_TRUE(identical(lin1, lin2));
    EXPECT_FALSE(identical(lin1, hold));

    const Interpolation ease1 = Interpolation::easing(EasingKind::EaseInQuad);
    const Interpolation ease2 = Interpolation::easing(EasingKind::EaseInQuad);
    const Interpolation ease3 = Interpolation::easing(EasingKind::EaseOutQuad);
    EXPECT_TRUE(identical(ease1, ease2));
    EXPECT_FALSE(identical(ease1, ease3));

    const Interpolation bez1 = Interpolation::bezier(0.2, 0.2, 0.8, 0.8).value();
    const Interpolation bez2 = Interpolation::bezier(0.2, 0.2, 0.8, 0.8).value();
    const Interpolation bez3 = Interpolation::bezier(0.3, 0.2, 0.8, 0.8).value();
    EXPECT_TRUE(identical(bez1, bez2));
    EXPECT_FALSE(identical(bez1, bez3));

    // -0.0 vs 0.0 bit difference
    const Interpolation bez_zero = Interpolation::bezier(0.0, 0.0, 1.0, 1.0).value();
    const Interpolation bez_neg_zero = Interpolation::bezier(-0.0, 0.0, 1.0, 1.0).value();
    EXPECT_FALSE(identical(bez_zero, bez_neg_zero));
}

}  // namespace
}  // namespace nxtcut::keyframes
