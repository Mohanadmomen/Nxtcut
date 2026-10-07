#include <nxtcut/core/mul_div.hpp>

#include <gtest/gtest.h>

#include <cstdint>

namespace nxtcut::core {
namespace {

TEST(MulDivTest, DivideByZeroReturnsInvalidArgument) {
    const auto res = mul_div(10, 5, 0, RoundingMode::Nearest);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), ErrorCode::InvalidArgument);
}

TEST(MulDivTest, ZeroOperandsReturnZero) {
    EXPECT_EQ(mul_div(0, 100, 5, RoundingMode::Nearest).value(), 0);
    EXPECT_EQ(mul_div(100, 0, 5, RoundingMode::Floor).value(), 0);
    EXPECT_EQ(mul_div(0, -100, 5, RoundingMode::Ceil).value(), 0);
}

TEST(MulDivTest, ExactDivisionAcrossAllModes) {
    for (const auto mode : {RoundingMode::Floor, RoundingMode::Nearest, RoundingMode::Ceil}) {
        EXPECT_EQ(mul_div(10, 6, 2, mode).value(), 30);
        EXPECT_EQ(mul_div(-10, 6, 2, mode).value(), -30);
        EXPECT_EQ(mul_div(10, -6, 2, mode).value(), -30);
        EXPECT_EQ(mul_div(-10, -6, 2, mode).value(), 30);
        EXPECT_EQ(mul_div(10, 6, -2, mode).value(), -30);
    }
}

TEST(MulDivTest, InexactPositiveRoundingModes) {
    // 7 / 5 = 1.4
    EXPECT_EQ(mul_div(7, 1, 5, RoundingMode::Floor).value(), 1);
    EXPECT_EQ(mul_div(7, 1, 5, RoundingMode::Nearest).value(), 1);
    EXPECT_EQ(mul_div(7, 1, 5, RoundingMode::Ceil).value(), 2);

    // 8 / 5 = 1.6
    EXPECT_EQ(mul_div(8, 1, 5, RoundingMode::Floor).value(), 1);
    EXPECT_EQ(mul_div(8, 1, 5, RoundingMode::Nearest).value(), 2);
    EXPECT_EQ(mul_div(8, 1, 5, RoundingMode::Ceil).value(), 2);

    // Half-way: 3 / 2 = 1.5
    EXPECT_EQ(mul_div(3, 1, 2, RoundingMode::Floor).value(), 1);
    EXPECT_EQ(mul_div(3, 1, 2, RoundingMode::Nearest).value(), 2);
    EXPECT_EQ(mul_div(3, 1, 2, RoundingMode::Ceil).value(), 2);

    // Half-way: 1 / 2 = 0.5
    EXPECT_EQ(mul_div(1, 1, 2, RoundingMode::Floor).value(), 0);
    EXPECT_EQ(mul_div(1, 1, 2, RoundingMode::Nearest).value(), 1);
    EXPECT_EQ(mul_div(1, 1, 2, RoundingMode::Ceil).value(), 1);
}

TEST(MulDivTest, InexactNegativeRoundingModes) {
    // -7 / 5 = -1.4
    EXPECT_EQ(mul_div(-7, 1, 5, RoundingMode::Floor).value(), -2);
    EXPECT_EQ(mul_div(-7, 1, 5, RoundingMode::Nearest).value(), -1);
    EXPECT_EQ(mul_div(-7, 1, 5, RoundingMode::Ceil).value(), -1);

    // -8 / 5 = -1.6
    EXPECT_EQ(mul_div(-8, 1, 5, RoundingMode::Floor).value(), -2);
    EXPECT_EQ(mul_div(-8, 1, 5, RoundingMode::Nearest).value(), -2);
    EXPECT_EQ(mul_div(-8, 1, 5, RoundingMode::Ceil).value(), -1);

    // Half-way: -3 / 2 = -1.5 (rounds half away from zero: -2)
    EXPECT_EQ(mul_div(-3, 1, 2, RoundingMode::Floor).value(), -2);
    EXPECT_EQ(mul_div(-3, 1, 2, RoundingMode::Nearest).value(), -2);
    EXPECT_EQ(mul_div(-3, 1, 2, RoundingMode::Ceil).value(), -1);

    // Half-way: -1 / 2 = -0.5
    EXPECT_EQ(mul_div(-1, 1, 2, RoundingMode::Floor).value(), -1);
    EXPECT_EQ(mul_div(-1, 1, 2, RoundingMode::Nearest).value(), -1);
    EXPECT_EQ(mul_div(-1, 1, 2, RoundingMode::Ceil).value(), 0);
}

TEST(MulDivTest, OperandsProductExceedingInt64) {
    // a * b = 20,000,000,000 * 20,000,000,000 = 4 * 10^20 > 2^63 - 1 (~9.22 * 10^18)
    // intermediate requires 128-bit math; result is 10,000,000,000 which fits in int64
    const std::int64_t a = 20'000'000'000LL;
    const std::int64_t b = 20'000'000'000LL;
    const std::int64_t c = 40'000'000'000LL;

    const auto res_pos = mul_div(a, b, c, RoundingMode::Nearest);
    ASSERT_TRUE(res_pos.has_value());
    EXPECT_EQ(*res_pos, 10'000'000'000LL);

    const auto res_neg = mul_div(-a, b, c, RoundingMode::Nearest);
    ASSERT_TRUE(res_neg.has_value());
    EXPECT_EQ(*res_neg, -10'000'000'000LL);
}

TEST(MulDivTest, OverflowDetection) {
    // Result exceeding int64 max
    const auto res1 = mul_div(INT64_MAX, 2, 1, RoundingMode::Nearest);
    ASSERT_FALSE(res1.has_value());
    EXPECT_EQ(res1.error().code(), ErrorCode::Overflow);

    // Result exceeding int64 min magnitude (-INT64_MIN overflows)
    const auto res2 = mul_div(INT64_MIN, -1, 1, RoundingMode::Nearest);
    ASSERT_FALSE(res2.has_value());
    EXPECT_EQ(res2.error().code(), ErrorCode::Overflow);

    // Large square with small divisor
    const auto res3 =
        mul_div(1'000'000'000'000'000'000LL, 1'000'000'000'000'000'000LL, 1, RoundingMode::Nearest);
    ASSERT_FALSE(res3.has_value());
    EXPECT_EQ(res3.error().code(), ErrorCode::Overflow);
}

TEST(MulDivTest, BoundaryValues) {
    // INT64_MIN * 1 / 1 == INT64_MIN
    const auto res_min = mul_div(INT64_MIN, 1, 1, RoundingMode::Nearest);
    ASSERT_TRUE(res_min.has_value());
    EXPECT_EQ(*res_min, INT64_MIN);

    // INT64_MAX * 1 / 1 == INT64_MAX
    const auto res_max = mul_div(INT64_MAX, 1, 1, RoundingMode::Nearest);
    ASSERT_TRUE(res_max.has_value());
    EXPECT_EQ(*res_max, INT64_MAX);
}

}  // namespace
}  // namespace nxtcut::core
