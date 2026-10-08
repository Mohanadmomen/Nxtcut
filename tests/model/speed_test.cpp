#include <nxtcut/model/speed.hpp>

#include <gtest/gtest.h>

namespace nxtcut::model {
namespace {

TEST(SpeedTest, DefaultAndNormal) {
    const Speed default_speed;
    const Speed normal_speed = Speed::normal();

    EXPECT_EQ(default_speed.numerator(), 1);
    EXPECT_EQ(default_speed.denominator(), 1);
    EXPECT_EQ(normal_speed.numerator(), 1);
    EXPECT_EQ(normal_speed.denominator(), 1);
    EXPECT_EQ(default_speed, normal_speed);
}

TEST(SpeedTest, ReductionByGcd) {
    const auto s = Speed::create(2, 4);
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->numerator(), 1);
    EXPECT_EQ(s->denominator(), 2);

    const auto s6_9 = Speed::create(6, 9);
    ASSERT_TRUE(s6_9.has_value());
    EXPECT_EQ(s6_9->numerator(), 2);
    EXPECT_EQ(s6_9->denominator(), 3);

    const auto s_double = Speed::create(2, 1);
    ASSERT_TRUE(s_double.has_value());
    EXPECT_EQ(s_double->numerator(), 2);
    EXPECT_EQ(s_double->denominator(), 1);
}

TEST(SpeedTest, InvalidArgumentsRejectZeroAndNegative) {
    const auto s_zero_num = Speed::create(0, 1);
    EXPECT_FALSE(s_zero_num.has_value());
    EXPECT_EQ(s_zero_num.error().code(), core::ErrorCode::InvalidArgument);

    const auto s_zero_den = Speed::create(1, 0);
    EXPECT_FALSE(s_zero_den.has_value());
    EXPECT_EQ(s_zero_den.error().code(), core::ErrorCode::InvalidArgument);

    const auto s_neg_num = Speed::create(-1, 2);
    EXPECT_FALSE(s_neg_num.has_value());
    EXPECT_EQ(s_neg_num.error().code(), core::ErrorCode::InvalidArgument);

    const auto s_neg_den = Speed::create(2, -1);
    EXPECT_FALSE(s_neg_den.has_value());
    EXPECT_EQ(s_neg_den.error().code(), core::ErrorCode::InvalidArgument);

    const auto s_both_neg = Speed::create(-2, -4);
    EXPECT_FALSE(s_both_neg.has_value());
    EXPECT_EQ(s_both_neg.error().code(), core::ErrorCode::InvalidArgument);
}

TEST(SpeedTest, Equality) {
    const auto half1 = Speed::create(1, 2).value();
    const auto half2 = Speed::create(2, 4).value();
    const auto normal = Speed::normal();
    const auto s3_2 = Speed::create(3, 2).value();
    const auto s2_3 = Speed::create(2, 3).value();

    EXPECT_EQ(half2, half1);
    EXPECT_NE(half1, normal);
    EXPECT_NE(s3_2, s2_3);
}

}  // namespace
}  // namespace nxtcut::model
