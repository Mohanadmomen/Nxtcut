#include <nxtcut/model/checked_arithmetic.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <optional>

namespace nxtcut::model::detail {
namespace {

constexpr std::int64_t kMax = std::numeric_limits<std::int64_t>::max();
constexpr std::int64_t kMin = std::numeric_limits<std::int64_t>::min();

// Compile-time constexpr usability assertions
static_assert(checked_add(1, 2) == std::optional<std::int64_t>{3});
static_assert(checked_add(kMax, 1) == std::nullopt);
static_assert(checked_sub(5, 3) == std::optional<std::int64_t>{2});
static_assert(checked_sub(0, kMin) == std::nullopt);

TEST(CheckedArithmeticTest, ConstexprUsability) {
    constexpr auto add_res = checked_add(1, 2);
    static_assert(add_res.has_value() && *add_res == 3);

    constexpr auto add_ovf = checked_add(kMax, 1);
    static_assert(!add_ovf.has_value());

    constexpr auto sub_res = checked_sub(5, 3);
    static_assert(sub_res.has_value() && *sub_res == 2);

    constexpr auto sub_ovf = checked_sub(0, kMin);
    static_assert(!sub_ovf.has_value());

    EXPECT_EQ(add_res, 3);
    EXPECT_EQ(add_ovf, std::nullopt);
    EXPECT_EQ(sub_res, 2);
    EXPECT_EQ(sub_ovf, std::nullopt);
}

TEST(CheckedArithmeticTest, CheckedAddRequiredVectors) {
    EXPECT_EQ(checked_add(5, 3), std::optional<std::int64_t>{8});
    EXPECT_EQ(checked_add(-5, 3), std::optional<std::int64_t>{-2});
    EXPECT_EQ(checked_add(kMax, 0), std::optional<std::int64_t>{kMax});
    EXPECT_EQ(checked_add(kMax, 1), std::nullopt);
    EXPECT_EQ(checked_add(kMax, -1), std::optional<std::int64_t>{kMax - 1});
    EXPECT_EQ(checked_add(kMin, 0), std::optional<std::int64_t>{kMin});
    EXPECT_EQ(checked_add(kMin, -1), std::nullopt);
    EXPECT_EQ(checked_add(kMin, 1), std::optional<std::int64_t>{kMin + 1});
    EXPECT_EQ(checked_add(kMin, kMax), std::optional<std::int64_t>{-1});
    EXPECT_EQ(checked_add(kMax, kMin), std::optional<std::int64_t>{-1});
    EXPECT_EQ(checked_add(kMax, kMax), std::nullopt);
    EXPECT_EQ(checked_add(kMin, kMin), std::nullopt);
}

TEST(CheckedArithmeticTest, CheckedSubRequiredVectors) {
    EXPECT_EQ(checked_sub(5, 3), std::optional<std::int64_t>{2});
    EXPECT_EQ(checked_sub(3, 5), std::optional<std::int64_t>{-2});
    EXPECT_EQ(checked_sub(0, kMin), std::nullopt);
    EXPECT_EQ(checked_sub(0, kMax), std::optional<std::int64_t>{-kMax});
    EXPECT_EQ(checked_sub(kMax, -1), std::nullopt);
    EXPECT_EQ(checked_sub(kMin, -1), std::optional<std::int64_t>{kMin + 1});
    EXPECT_EQ(checked_sub(kMin, 1), std::nullopt);
    EXPECT_EQ(checked_sub(kMax, kMax), std::optional<std::int64_t>{0});
    EXPECT_EQ(checked_sub(kMin, kMin), std::optional<std::int64_t>{0});
    EXPECT_EQ(checked_sub(-1, kMin), std::optional<std::int64_t>{kMax});
    EXPECT_EQ(checked_sub(kMin, kMax), std::nullopt);
}

}  // namespace
}  // namespace nxtcut::model::detail
