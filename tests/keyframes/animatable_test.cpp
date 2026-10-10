#include <nxtcut/core/color.hpp>
#include <nxtcut/core/geometry.hpp>
#include <nxtcut/keyframes/animatable.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <string>

namespace nxtcut::keyframes {
namespace {

// Static assertions ensuring Animatable concept satisfaction
static_assert(Animatable<double>);
static_assert(Animatable<core::Color>);
static_assert(Animatable<core::Point<double>>);
static_assert(!Animatable<int>);
static_assert(!Animatable<std::string>);

TEST(AnimatableTest, DoubleInterpolationAndTraits) {
    EXPECT_EQ(AnimatableTraits<double>::default_value(), 0.0);

    const double a = 10.0;
    const double b = 20.0;
    EXPECT_EQ(AnimatableTraits<double>::interpolate(a, b, 0.0), 10.0);
    EXPECT_EQ(AnimatableTraits<double>::interpolate(a, b, 0.5), 15.0);
    EXPECT_EQ(AnimatableTraits<double>::interpolate(a, b, 1.0), 20.0);
    EXPECT_EQ(AnimatableTraits<double>::interpolate(a, b, -0.5), 5.0);
    EXPECT_EQ(AnimatableTraits<double>::interpolate(a, b, 1.5), 25.0);

    EXPECT_TRUE(AnimatableTraits<double>::is_finite(0.0));
    EXPECT_FALSE(AnimatableTraits<double>::is_finite(std::numeric_limits<double>::quiet_NaN()));
    EXPECT_FALSE(AnimatableTraits<double>::is_finite(std::numeric_limits<double>::infinity()));

    EXPECT_TRUE(AnimatableTraits<double>::identical(1.0, 1.0));
    EXPECT_FALSE(AnimatableTraits<double>::identical(0.0, -0.0));
}

TEST(AnimatableTest, ColorInterpolationAndTraits) {
    const core::Color def = AnimatableTraits<core::Color>::default_value();
    EXPECT_EQ(def.r, 0.0f);
    EXPECT_EQ(def.g, 0.0f);
    EXPECT_EQ(def.b, 0.0f);
    EXPECT_EQ(def.a, 1.0f);

    const core::Color red{1.0f, 0.0f, 0.0f, 1.0f};
    const core::Color blue{0.0f, 0.0f, 1.0f, 1.0f};

    const core::Color at_zero = AnimatableTraits<core::Color>::interpolate(red, blue, 0.0);
    EXPECT_EQ(at_zero.r, red.r);
    EXPECT_EQ(at_zero.g, red.g);
    EXPECT_EQ(at_zero.b, red.b);
    EXPECT_EQ(at_zero.a, red.a);

    const core::Color at_half = AnimatableTraits<core::Color>::interpolate(red, blue, 0.5);
    EXPECT_FLOAT_EQ(at_half.r, 0.5f);
    EXPECT_FLOAT_EQ(at_half.g, 0.0f);
    EXPECT_FLOAT_EQ(at_half.b, 0.5f);
    EXPECT_FLOAT_EQ(at_half.a, 1.0f);

    const core::Color at_one = AnimatableTraits<core::Color>::interpolate(red, blue, 1.0);
    EXPECT_EQ(at_one.r, blue.r);
    EXPECT_EQ(at_one.g, blue.g);
    EXPECT_EQ(at_one.b, blue.b);
    EXPECT_EQ(at_one.a, blue.a);

    EXPECT_TRUE(AnimatableTraits<core::Color>::is_finite(red));
    core::Color nan_color = red;
    nan_color.r = std::numeric_limits<float>::quiet_NaN();
    EXPECT_FALSE(AnimatableTraits<core::Color>::is_finite(nan_color));

    core::Color inf_color = red;
    inf_color.a = std::numeric_limits<float>::infinity();
    EXPECT_FALSE(AnimatableTraits<core::Color>::is_finite(inf_color));

    EXPECT_TRUE(AnimatableTraits<core::Color>::identical(red, red));
    const core::Color zero_c{0.0f, 0.0f, 0.0f, 0.0f};
    const core::Color neg_zero_c{-0.0f, 0.0f, 0.0f, 0.0f};
    EXPECT_FALSE(AnimatableTraits<core::Color>::identical(zero_c, neg_zero_c));
}

TEST(AnimatableTest, PointInterpolationAndTraits) {
    const core::Point<double> def = AnimatableTraits<core::Point<double>>::default_value();
    EXPECT_EQ(def.x, 0.0);
    EXPECT_EQ(def.y, 0.0);

    const core::Point<double> p1{10.0, 20.0};
    const core::Point<double> p2{30.0, 40.0};

    const auto at_zero = AnimatableTraits<core::Point<double>>::interpolate(p1, p2, 0.0);
    EXPECT_EQ(at_zero.x, 10.0);
    EXPECT_EQ(at_zero.y, 20.0);

    const auto at_half = AnimatableTraits<core::Point<double>>::interpolate(p1, p2, 0.5);
    EXPECT_DOUBLE_EQ(at_half.x, 20.0);
    EXPECT_DOUBLE_EQ(at_half.y, 30.0);

    const auto at_one = AnimatableTraits<core::Point<double>>::interpolate(p1, p2, 1.0);
    EXPECT_EQ(at_one.x, 30.0);
    EXPECT_EQ(at_one.y, 40.0);

    EXPECT_TRUE(AnimatableTraits<core::Point<double>>::is_finite(p1));
    core::Point<double> nan_p{std::numeric_limits<double>::quiet_NaN(), 0.0};
    EXPECT_FALSE(AnimatableTraits<core::Point<double>>::is_finite(nan_p));

    core::Point<double> inf_p{0.0, std::numeric_limits<double>::infinity()};
    EXPECT_FALSE(AnimatableTraits<core::Point<double>>::is_finite(inf_p));

    EXPECT_TRUE(AnimatableTraits<core::Point<double>>::identical(p1, p1));
    const core::Point<double> z_p{0.0, 0.0};
    const core::Point<double> nz_p{-0.0, 0.0};
    EXPECT_FALSE(AnimatableTraits<core::Point<double>>::identical(z_p, nz_p));
}

}  // namespace
}  // namespace nxtcut::keyframes
