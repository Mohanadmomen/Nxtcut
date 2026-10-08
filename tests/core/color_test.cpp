#include <nxtcut/core/color.hpp>

#include <gtest/gtest.h>

#include <cmath>

namespace nxtcut::core {
namespace {

TEST(ColorTest, RequiredTransferFunctions) {
    EXPECT_NEAR(srgb_to_linear(0.0f), 0.0f, 1e-7f);
    EXPECT_NEAR(srgb_to_linear(1.0f), 1.0f, 1e-7f);
    EXPECT_NEAR(srgb_to_linear(0.5f), 0.2140411f, 1e-6f);
    EXPECT_NEAR(srgb_to_linear(0.04045f), 0.04045f / 12.92f, 1e-7f);

    EXPECT_NEAR(linear_to_srgb(0.0f), 0.0f, 1e-7f);
    EXPECT_NEAR(linear_to_srgb(1.0f), 1.0f, 1e-7f);
    EXPECT_NEAR(linear_to_srgb(0.5f), 0.7353570f, 1e-6f);
    EXPECT_NEAR(linear_to_srgb(0.0031308f), 0.04045f, 1e-5f);
}

TEST(ColorTest, Required8BitRoundTripInvertibility) {
    for (int i = 0; i <= 255; ++i) {
        const float original = static_cast<float>(i) / 255.0f;
        const float linear = srgb_to_linear(original);
        const float roundtrip = linear_to_srgb(linear);
        const int reconstructed = static_cast<int>(std::lround(roundtrip * 255.0f));
        EXPECT_EQ(reconstructed, i);
    }
}

TEST(ColorTest, RequiredHexVectors) {
    const auto c1 = Color::from_hex("#FF8000");
    ASSERT_TRUE(c1.has_value());
    EXPECT_NEAR(c1->r, 1.0f, 1e-6f);
    EXPECT_NEAR(c1->g, 128.0f / 255.0f, 1e-6f);
    EXPECT_NEAR(c1->b, 0.0f, 1e-6f);
    EXPECT_NEAR(c1->a, 1.0f, 1e-6f);

    const auto c2 = Color::from_hex("#f80");
    ASSERT_TRUE(c2.has_value());
    EXPECT_NEAR(c2->r, 1.0f, 1e-6f);
    EXPECT_NEAR(c2->g, 136.0f / 255.0f, 1e-6f);
    EXPECT_NEAR(c2->b, 0.0f, 1e-6f);
    EXPECT_NEAR(c2->a, 1.0f, 1e-6f);

    const auto c3 = Color::from_hex("#336699CC");
    ASSERT_TRUE(c3.has_value());
    EXPECT_EQ(c3->to_hex(), "#336699cc");
    EXPECT_EQ(c3->to_hex(false), "#336699");
}

TEST(ColorTest, RequiredInvalidHexInputs) {
    const char* const invalid_inputs[] = {
        "", "#", "FF8000", "#GG0000", "#12345", "#1234567", "#123456789",
    };
    for (const char* invalid : invalid_inputs) {
        const auto res = Color::from_hex(invalid);
        EXPECT_FALSE(res.has_value()) << "Expected failure for: " << invalid;
        EXPECT_EQ(res.error().code(), ErrorCode::InvalidArgument);
    }
}

TEST(ColorTest, RequiredPremultipliedVectors) {
    const Color c{1.0f, 0.5f, 0.25f, 0.5f};
    const PremultipliedColor pm = c.premultiplied();
    EXPECT_NEAR(pm.r, 0.5f, 1e-6f);
    EXPECT_NEAR(pm.g, 0.25f, 1e-6f);
    EXPECT_NEAR(pm.b, 0.125f, 1e-6f);
    EXPECT_NEAR(pm.a, 0.5f, 1e-6f);

    const Color unpm = pm.unpremultiplied();
    EXPECT_NEAR(unpm.r, c.r, 1e-6f);
    EXPECT_NEAR(unpm.g, c.g, 1e-6f);
    EXPECT_NEAR(unpm.b, c.b, 1e-6f);
    EXPECT_NEAR(unpm.a, c.a, 1e-6f);

    const PremultipliedColor zero_alpha{0.8f, 0.4f, 0.2f, 0.0f};
    const Color unpm_zero = zero_alpha.unpremultiplied();
    EXPECT_EQ(unpm_zero.r, 0.0f);
    EXPECT_EQ(unpm_zero.g, 0.0f);
    EXPECT_EQ(unpm_zero.b, 0.0f);
    EXPECT_EQ(unpm_zero.a, 0.0f);
}

TEST(ColorTest, RequiredLerpVectors) {
    const Color black{0.0f, 0.0f, 0.0f, 1.0f};
    const Color white{1.0f, 1.0f, 1.0f, 1.0f};

    const Color quarter = lerp(black, white, 0.25f);
    EXPECT_NEAR(quarter.r, 0.25f, 1e-6f);
    EXPECT_NEAR(quarter.g, 0.25f, 1e-6f);
    EXPECT_NEAR(quarter.b, 0.25f, 1e-6f);
    EXPECT_NEAR(quarter.a, 1.0f, 1e-6f);

    const Color t0 = lerp(black, white, 0.0f);
    EXPECT_EQ(t0.r, black.r);
    EXPECT_EQ(t0.g, black.g);
    EXPECT_EQ(t0.b, black.b);
    EXPECT_EQ(t0.a, black.a);

    const Color t1 = lerp(black, white, 1.0f);
    EXPECT_EQ(t1.r, white.r);
    EXPECT_EQ(t1.g, white.g);
    EXPECT_EQ(t1.b, white.b);
    EXPECT_EQ(t1.a, white.a);
}

}  // namespace
}  // namespace nxtcut::core
