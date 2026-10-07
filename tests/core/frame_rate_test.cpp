#include <nxtcut/core/frame_rate.hpp>

#include <gtest/gtest.h>

namespace nxtcut::core {
namespace {

TEST(FrameRateTest, InvalidInputRejection) {
    EXPECT_FALSE(FrameRate::create(0, 1).has_value());
    EXPECT_FALSE(FrameRate::create(24, 0).has_value());
    EXPECT_FALSE(FrameRate::create(-24, 1).has_value());
    EXPECT_FALSE(FrameRate::create(24, -1).has_value());
    EXPECT_FALSE(FrameRate::create(-30, -1).has_value());
}

TEST(FrameRateTest, NormalizationWithGcd) {
    const auto r1 = FrameRate::create(48, 2);
    ASSERT_TRUE(r1.has_value());
    EXPECT_EQ(r1->numerator(), 24);
    EXPECT_EQ(r1->denominator(), 1);

    const auto r2 = FrameRate::create(60000, 2002);
    ASSERT_TRUE(r2.has_value());
    EXPECT_EQ(r2->numerator(), 30000);
    EXPECT_EQ(r2->denominator(), 1001);
}

TEST(FrameRateTest, StandardConstantsAndProperties) {
    using namespace frame_rates;

    EXPECT_EQ(k23_976.numerator(), 24000);
    EXPECT_EQ(k23_976.denominator(), 1001);
    EXPECT_EQ(k23_976.nominal_fps(), 24);
    EXPECT_TRUE(k23_976.is_ntsc());
    EXPECT_EQ(k23_976.to_string(), "23.976");

    EXPECT_EQ(k24.numerator(), 24);
    EXPECT_EQ(k24.denominator(), 1);
    EXPECT_EQ(k24.nominal_fps(), 24);
    EXPECT_FALSE(k24.is_ntsc());
    EXPECT_EQ(k24.to_string(), "24");

    EXPECT_EQ(k25.numerator(), 25);
    EXPECT_EQ(k25.denominator(), 1);
    EXPECT_EQ(k25.nominal_fps(), 25);
    EXPECT_FALSE(k25.is_ntsc());
    EXPECT_EQ(k25.to_string(), "25");

    EXPECT_EQ(k29_97.numerator(), 30000);
    EXPECT_EQ(k29_97.denominator(), 1001);
    EXPECT_EQ(k29_97.nominal_fps(), 30);
    EXPECT_TRUE(k29_97.is_ntsc());
    EXPECT_EQ(k29_97.to_string(), "29.97");

    EXPECT_EQ(k30.numerator(), 30);
    EXPECT_EQ(k30.denominator(), 1);
    EXPECT_EQ(k30.nominal_fps(), 30);
    EXPECT_FALSE(k30.is_ntsc());
    EXPECT_EQ(k30.to_string(), "30");

    EXPECT_EQ(k50.numerator(), 50);
    EXPECT_EQ(k50.denominator(), 1);
    EXPECT_EQ(k50.nominal_fps(), 50);
    EXPECT_FALSE(k50.is_ntsc());
    EXPECT_EQ(k50.to_string(), "50");

    EXPECT_EQ(k59_94.numerator(), 60000);
    EXPECT_EQ(k59_94.denominator(), 1001);
    EXPECT_EQ(k59_94.nominal_fps(), 60);
    EXPECT_TRUE(k59_94.is_ntsc());
    EXPECT_EQ(k59_94.to_string(), "59.94");

    EXPECT_EQ(k60.numerator(), 60);
    EXPECT_EQ(k60.denominator(), 1);
    EXPECT_EQ(k60.nominal_fps(), 60);
    EXPECT_FALSE(k60.is_ntsc());
    EXPECT_EQ(k60.to_string(), "60");
}

TEST(FrameRateTest, ComparisonAndOrderingExactFraction) {
    using namespace frame_rates;

    EXPECT_LT(k23_976, k24);
    EXPECT_LT(k24, k25);
    EXPECT_LT(k25, k29_97);
    EXPECT_LT(k29_97, k30);
    EXPECT_LT(k30, k50);
    EXPECT_LT(k50, k59_94);
    EXPECT_LT(k59_94, k60);

    const auto created_24 = FrameRate::create(48, 2).value();
    EXPECT_EQ(created_24, k24);
    EXPECT_LE(created_24, k24);
    EXPECT_GE(created_24, k24);
}

TEST(SampleRateTest, CreationAndValidation) {
    EXPECT_FALSE(SampleRate::create(0).has_value());
    EXPECT_FALSE(SampleRate::create(-44100).has_value());

    const auto s48 = SampleRate::create(48000);
    ASSERT_TRUE(s48.has_value());
    EXPECT_EQ(s48->value(), 48000);
}

TEST(SampleRateTest, StandardConstants) {
    using namespace sample_rates;

    EXPECT_EQ(k44100.value(), 44100);
    EXPECT_EQ(k48000.value(), 48000);
    EXPECT_EQ(k96000.value(), 96000);

    EXPECT_LT(k44100, k48000);
    EXPECT_LT(k48000, k96000);
}

}  // namespace
}  // namespace nxtcut::core
