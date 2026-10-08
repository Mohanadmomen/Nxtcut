#include <nxtcut/core/mul_div.hpp>
#include <nxtcut/core/mul_div_detail.hpp>

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

void expect_both_success(std::int64_t a, std::int64_t b, std::int64_t c, RoundingMode mode,
                         std::int64_t expected) {
    SCOPED_TRACE(::testing::Message()
                 << "a=" << a << " b=" << b << " c=" << c << " mode=" << static_cast<int>(mode));
    const auto res_fast = mul_div(a, b, c, mode);
    ASSERT_TRUE(res_fast.has_value());
    EXPECT_EQ(*res_fast, expected);

    const auto res_port = detail::mul_div_portable(a, b, c, mode);
    ASSERT_TRUE(res_port.has_value());
    EXPECT_EQ(*res_port, expected);
}

void expect_both_error(std::int64_t a, std::int64_t b, std::int64_t c, RoundingMode mode,
                       ErrorCode expected_err) {
    SCOPED_TRACE(::testing::Message()
                 << "a=" << a << " b=" << b << " c=" << c << " mode=" << static_cast<int>(mode));
    const auto res_fast = mul_div(a, b, c, mode);
    ASSERT_FALSE(res_fast.has_value());
    EXPECT_EQ(res_fast.error().code(), expected_err);

    const auto res_port = detail::mul_div_portable(a, b, c, mode);
    ASSERT_FALSE(res_port.has_value());
    EXPECT_EQ(res_port.error().code(), expected_err);
}

void check_equivalence(std::int64_t a, std::int64_t b, std::int64_t c, RoundingMode mode) {
    SCOPED_TRACE(::testing::Message()
                 << "a=" << a << " b=" << b << " c=" << c << " mode=" << static_cast<int>(mode));
    const auto res_fast = mul_div(a, b, c, mode);
    const auto res_port = detail::mul_div_portable(a, b, c, mode);

    const bool fast_ok = res_fast.has_value();
    const bool port_ok = res_port.has_value();
    ASSERT_EQ(fast_ok, port_ok);
    if (fast_ok && port_ok) {
        EXPECT_EQ(*res_fast, *res_port);
    } else if (!fast_ok && !port_ok) {
        EXPECT_EQ(res_fast.error().code(), res_port.error().code());
    }
}

TEST(MulDivTest, IndependentKnownVectors) {
    constexpr RoundingMode all_modes[] = {
        RoundingMode::Floor,
        RoundingMode::Nearest,
        RoundingMode::Ceil,
    };

    // (INT64_MAX, INT64_MAX, INT64_MAX, Floor) = INT64_MAX
    expect_both_success(INT64_MAX, INT64_MAX, INT64_MAX, RoundingMode::Floor, INT64_MAX);

    // (INT64_MIN, 1, 1, Floor) = INT64_MIN
    expect_both_success(INT64_MIN, 1, 1, RoundingMode::Floor, INT64_MIN);

    // (INT64_MIN, -1, 1, any mode) = Overflow
    for (const auto mode : all_modes) {
        expect_both_error(INT64_MIN, -1, 1, mode, ErrorCode::Overflow);
    }

    // (INT64_MIN, INT64_MIN, INT64_MIN, Floor) = INT64_MIN
    expect_both_success(INT64_MIN, INT64_MIN, INT64_MIN, RoundingMode::Floor, INT64_MIN);

    // (INT64_MAX, 2, 2, Floor) = INT64_MAX
    expect_both_success(INT64_MAX, 2, 2, RoundingMode::Floor, INT64_MAX);

    // (INT64_MAX, INT64_MAX, 1, any mode) = Overflow
    for (const auto mode : all_modes) {
        expect_both_error(INT64_MAX, INT64_MAX, 1, mode, ErrorCode::Overflow);
    }

    // (1,1,2): Floor=0, Nearest=1, Ceil=1
    expect_both_success(1, 1, 2, RoundingMode::Floor, 0);
    expect_both_success(1, 1, 2, RoundingMode::Nearest, 1);
    expect_both_success(1, 1, 2, RoundingMode::Ceil, 1);

    // (-1,1,2): Floor=-1, Nearest=-1, Ceil=0
    expect_both_success(-1, 1, 2, RoundingMode::Floor, -1);
    expect_both_success(-1, 1, 2, RoundingMode::Nearest, -1);
    expect_both_success(-1, 1, 2, RoundingMode::Ceil, 0);

    // (3,1,2): Nearest=2; (-3,1,2): Nearest=-2
    expect_both_success(3, 1, 2, RoundingMode::Nearest, 2);
    expect_both_success(-3, 1, 2, RoundingMode::Nearest, -2);

    // (2^62, 4, 2^61, any mode) = 8
    const std::int64_t pow2_62 = INT64_C(1) << 62;
    const std::int64_t pow2_61 = INT64_C(1) << 61;
    for (const auto mode : all_modes) {
        expect_both_success(pow2_62, 4, pow2_61, mode, 8);
    }

    // (2^40, 2^40, 2^20, any mode) = 2^60
    const std::int64_t pow2_40 = INT64_C(1) << 40;
    const std::int64_t pow2_20 = INT64_C(1) << 20;
    const std::int64_t pow2_60 = INT64_C(1) << 60;
    for (const auto mode : all_modes) {
        expect_both_success(pow2_40, pow2_40, pow2_20, mode, pow2_60);
    }

    // (5, 1, 0, any mode) = InvalidArgument; (0, 5, 3, any mode) = 0
    for (const auto mode : all_modes) {
        expect_both_error(5, 1, 0, mode, ErrorCode::InvalidArgument);
        expect_both_success(0, 5, 3, mode, 0);
    }

    // result exactly +2^63 is Overflow: (INT64_MIN, -1, 1) covers this;
    // also a case whose result is exactly -2^63 succeeds: (INT64_MIN, 3, 3, Floor) = INT64_MIN.
    expect_both_success(INT64_MIN, 3, 3, RoundingMode::Floor, INT64_MIN);
}

TEST(MulDivTest, EquivalenceExhaustiveGrid) {
    constexpr RoundingMode all_modes[] = {
        RoundingMode::Floor,
        RoundingMode::Nearest,
        RoundingMode::Ceil,
    };

    const std::int64_t values[] = {
        INT64_MIN, INT64_MIN + 1, -4294967296LL, -4294967295LL, -3,        -2, -1, 1, 2,
        3,         4294967295LL,  4294967296LL,  INT64_MAX - 1, INT64_MAX,
    };

    for (const auto a : values) {
        for (const auto b : values) {
            for (const auto c : values) {
                for (const auto mode : all_modes) {
                    check_equivalence(a, b, c, mode);
                }
            }
        }
    }

    // Include zero separately
    for (const auto a : values) {
        for (const auto b : values) {
            for (const auto mode : all_modes) {
                check_equivalence(a, b, 0, mode);
            }
        }
    }

    for (const auto c : values) {
        for (const auto mode : all_modes) {
            check_equivalence(0, 0, c, mode);
            check_equivalence(0, 1, c, mode);
            check_equivalence(1, 0, c, mode);
        }
    }
    for (const auto mode : all_modes) {
        check_equivalence(0, 0, 0, mode);
    }
}

TEST(MulDivTest, EquivalenceDeterministicXorShift64) {
    constexpr RoundingMode all_modes[] = {
        RoundingMode::Floor,
        RoundingMode::Nearest,
        RoundingMode::Ceil,
    };

    struct XorShift64 {
        std::uint64_t state{0x9E3779B97F4A7C15ULL};

        constexpr std::uint64_t next() noexcept {
            std::uint64_t x = state;
            x ^= x << 13;
            x ^= x >> 7;
            x ^= x << 17;
            state = x;
            return x;
        }

        constexpr std::int64_t next_i64() noexcept { return static_cast<std::int64_t>(next()); }
    };

    XorShift64 rng;

    constexpr int kTripleCount = 20000;
    for (int i = 0; i < kTripleCount; ++i) {
        const std::int64_t a = rng.next_i64();
        const std::int64_t b = rng.next_i64();
        std::int64_t c = rng.next_i64();
        while (c == 0) {
            c = rng.next_i64();
        }

        for (const auto mode : all_modes) {
            check_equivalence(a, b, c, mode);
        }
    }
}

}  // namespace
}  // namespace nxtcut::core
