#include <nxtcut/keyframes/easing.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <limits>
#include <string_view>
#include <unordered_set>

namespace nxtcut::keyframes {
namespace {

constexpr std::array<EasingKind, 19> kAllKinds = {
    EasingKind::Linear,         EasingKind::EaseInSine,  EasingKind::EaseOutSine,
    EasingKind::EaseInOutSine,  EasingKind::EaseInQuad,  EasingKind::EaseOutQuad,
    EasingKind::EaseInOutQuad,  EasingKind::EaseInCubic, EasingKind::EaseOutCubic,
    EasingKind::EaseInOutCubic, EasingKind::EaseInQuart, EasingKind::EaseOutQuart,
    EasingKind::EaseInOutQuart, EasingKind::EaseInExpo,  EasingKind::EaseOutExpo,
    EasingKind::EaseInOutExpo,  EasingKind::EaseInCirc,  EasingKind::EaseOutCirc,
    EasingKind::EaseInOutCirc,
};

TEST(EasingTest, EveryKindEndpointsAndClamping) {
    const double nan_val = std::numeric_limits<double>::quiet_NaN();

    for (const auto kind : kAllKinds) {
        EXPECT_EQ(ease(kind, 0.0), 0.0);
        EXPECT_EQ(ease(kind, 1.0), 1.0);
        EXPECT_EQ(ease(kind, -0.5), 0.0);
        EXPECT_EQ(ease(kind, 1.5), 1.0);
        EXPECT_EQ(ease(kind, nan_val), 0.0);

        double prev = 0.0;
        for (int i = 0; i <= 1000; ++i) {
            const double t = static_cast<double>(i) / 1000.0;
            const double val = ease(kind, t);
            EXPECT_GE(val, 0.0);
            EXPECT_LE(val, 1.0);
            EXPECT_GE(val, prev);
            prev = val;
        }
    }
}

TEST(EasingTest, SymmetryPerFamily) {
    struct Family {
        EasingKind in;
        EasingKind out;
        EasingKind in_out;
    };

    const std::array<Family, 6> families = {{
        {EasingKind::EaseInSine, EasingKind::EaseOutSine, EasingKind::EaseInOutSine},
        {EasingKind::EaseInQuad, EasingKind::EaseOutQuad, EasingKind::EaseInOutQuad},
        {EasingKind::EaseInCubic, EasingKind::EaseOutCubic, EasingKind::EaseInOutCubic},
        {EasingKind::EaseInQuart, EasingKind::EaseOutQuart, EasingKind::EaseInOutQuart},
        {EasingKind::EaseInExpo, EasingKind::EaseOutExpo, EasingKind::EaseInOutExpo},
        {EasingKind::EaseInCirc, EasingKind::EaseOutCirc, EasingKind::EaseInOutCirc},
    }};

    const std::array<double, 3> test_points = {0.1, 0.25, 0.4};

    for (const auto& fam : families) {
        for (const double t : test_points) {
            const double out_val = ease(fam.out, t);
            const double in_inv = 1.0 - ease(fam.in, 1.0 - t);
            EXPECT_NEAR(out_val, in_inv, 1e-12);

            const double in_out_sum = ease(fam.in_out, t) + ease(fam.in_out, 1.0 - t);
            EXPECT_NEAR(in_out_sum, 1.0, 1e-12);
        }
    }
}

TEST(EasingTest, RequiredExactVectors) {
    EXPECT_NEAR(ease(EasingKind::EaseInQuad, 0.5), 0.25, 1e-9);
    EXPECT_NEAR(ease(EasingKind::EaseOutQuad, 0.5), 0.75, 1e-9);
    EXPECT_NEAR(ease(EasingKind::EaseInOutQuad, 0.25), 0.125, 1e-9);
    EXPECT_NEAR(ease(EasingKind::EaseInOutQuad, 0.75), 0.875, 1e-9);

    EXPECT_NEAR(ease(EasingKind::EaseInCubic, 0.5), 0.125, 1e-9);
    EXPECT_NEAR(ease(EasingKind::EaseOutCubic, 0.5), 0.875, 1e-9);
    EXPECT_NEAR(ease(EasingKind::EaseInOutCubic, 0.25), 0.0625, 1e-9);

    EXPECT_NEAR(ease(EasingKind::EaseInQuart, 0.5), 0.0625, 1e-9);

    EXPECT_NEAR(ease(EasingKind::EaseInSine, 0.5), 0.2928932188, 1e-9);
    EXPECT_NEAR(ease(EasingKind::EaseOutSine, 0.5), 0.7071067812, 1e-9);
    EXPECT_NEAR(ease(EasingKind::EaseInOutSine, 0.5), 0.5, 1e-9);

    EXPECT_NEAR(ease(EasingKind::EaseInExpo, 0.5), 0.03125, 1e-9);
    EXPECT_NEAR(ease(EasingKind::EaseOutExpo, 0.5), 0.96875, 1e-9);

    EXPECT_NEAR(ease(EasingKind::EaseInCirc, 0.5), 0.1339745962, 1e-9);
    EXPECT_NEAR(ease(EasingKind::EaseOutCirc, 0.5), 0.8660254038, 1e-9);

    // Every InOut kind at 0.5 must equal 0.5
    const std::array<EasingKind, 6> in_out_kinds = {
        EasingKind::EaseInOutSine,  EasingKind::EaseInOutQuad, EasingKind::EaseInOutCubic,
        EasingKind::EaseInOutQuart, EasingKind::EaseInOutExpo, EasingKind::EaseInOutCirc,
    };
    for (const auto kind : in_out_kinds) {
        EXPECT_NEAR(ease(kind, 0.5), 0.5, 1e-9);
    }
}

TEST(EasingTest, ToStringRoundTripAndUniqueness) {
    std::unordered_set<std::string_view> seen_strings;

    for (const auto kind : kAllKinds) {
        const std::string_view str = to_string(kind);
        EXPECT_FALSE(str.empty());
        EXPECT_TRUE(seen_strings.insert(str).second);

        const auto parsed = easing_from_string(str);
        ASSERT_TRUE(parsed.has_value());
        EXPECT_EQ(parsed.value(), kind);
    }

    EXPECT_EQ(seen_strings.size(), 19U);
    EXPECT_FALSE(easing_from_string("non-existent-easing").has_value());
    EXPECT_FALSE(easing_from_string("").has_value());
}

}  // namespace
}  // namespace nxtcut::keyframes
