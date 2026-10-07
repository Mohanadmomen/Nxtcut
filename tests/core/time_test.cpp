#include <nxtcut/core/time.hpp>

#include "compile_checks.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <type_traits>

namespace nxtcut::core {
namespace {

using test::CanAdd;
using test::CanSubtract;

// Compile-time static assertions ensuring strong typing prohibitions
static_assert(!CanAdd<TimePoint, TimePoint>);
static_assert(!CanAdd<TimePoint, std::int64_t>);
static_assert(!CanSubtract<TimePoint, std::int64_t>);
static_assert(!CanAdd<Duration, std::int64_t>);
static_assert(!CanSubtract<Duration, std::int64_t>);

// Positive compile checks ensuring valid operations are supported
static_assert(CanAdd<TimePoint, Duration>);
static_assert(CanAdd<Duration, TimePoint>);
static_assert(CanSubtract<TimePoint, Duration>);
static_assert(CanSubtract<TimePoint, TimePoint>);
static_assert(CanAdd<Duration, Duration>);
static_assert(CanSubtract<Duration, Duration>);
static_assert(!std::is_convertible_v<TimePoint, std::int64_t>, "TimePoint must not implicitly convert to int64");
static_assert(!std::is_convertible_v<std::int64_t, TimePoint>, "int64 must not implicitly convert to TimePoint");
static_assert(!std::is_convertible_v<Duration, std::int64_t>, "Duration must not implicitly convert to int64");
static_assert(!std::is_convertible_v<std::int64_t, Duration>, "int64 must not implicitly convert to Duration");

// Constexpr evaluation tests
constexpr Duration kConstDuration = Duration::from_ticks(100);
static_assert(kConstDuration.ticks() == 100);
static_assert((kConstDuration + Duration::from_ticks(50)).ticks() == 150);
static_assert((kConstDuration - Duration::from_ticks(30)).ticks() == 70);
static_assert((-kConstDuration).ticks() == -100);
static_assert((kConstDuration * 3).ticks() == 300);
static_assert((3 * kConstDuration).ticks() == 300);
static_assert((kConstDuration / 2).ticks() == 50);
static_assert(Duration::from_ticks(-40).abs().ticks() == 40);

constexpr TimePoint kConstTimePoint = TimePoint::from_ticks(1000);
static_assert((kConstTimePoint + kConstDuration).ticks() == 1100);
static_assert((kConstDuration + kConstTimePoint).ticks() == 1100);
static_assert((kConstTimePoint - kConstDuration).ticks() == 900);
static_assert((kConstTimePoint - TimePoint::from_ticks(800)).ticks() == 200);

TEST(TimeTest, DurationBasics) {
    const Duration d1 = Duration::from_ticks(705'600'000);
    EXPECT_EQ(d1.ticks(), 705'600'000);
    EXPECT_DOUBLE_EQ(d1.to_seconds(), 1.0);

    const Duration d2 = Duration::from_seconds(2.5);
    EXPECT_EQ(d2.ticks(), 1'764'000'000LL);
    EXPECT_DOUBLE_EQ(d2.to_seconds(), 2.5);

    const Duration d_zero = Duration::zero();
    EXPECT_EQ(d_zero.ticks(), 0);

    EXPECT_EQ(abs(Duration::from_ticks(-1234)).ticks(), 1234);
    EXPECT_EQ(Duration::from_ticks(-1234).abs().ticks(), 1234);
}

TEST(TimeTest, DurationCompoundAssignment) {
    Duration d = Duration::from_ticks(100);
    d += Duration::from_ticks(50);
    EXPECT_EQ(d.ticks(), 150);

    d -= Duration::from_ticks(20);
    EXPECT_EQ(d.ticks(), 130);

    d *= 2;
    EXPECT_EQ(d.ticks(), 260);

    d /= 4;
    EXPECT_EQ(d.ticks(), 65);
}

TEST(TimeTest, TimePointBasics) {
    const TimePoint tp1 = TimePoint::from_ticks(705'600'000);
    EXPECT_EQ(tp1.ticks(), 705'600'000);
    EXPECT_DOUBLE_EQ(tp1.to_seconds(), 1.0);

    const TimePoint tp_zero = TimePoint::zero();
    EXPECT_EQ(tp_zero.ticks(), 0);

    const Duration diff = tp1 - tp_zero;
    EXPECT_EQ(diff.ticks(), 705'600'000);

    TimePoint mut_tp = TimePoint::from_ticks(500);
    mut_tp += Duration::from_ticks(200);
    EXPECT_EQ(mut_tp.ticks(), 700);

    mut_tp -= Duration::from_ticks(300);
    EXPECT_EQ(mut_tp.ticks(), 400);
}

TEST(TimeTest, TimeRangeCreation) {
    const TimePoint start = TimePoint::from_ticks(100);
    const Duration dur = Duration::from_ticks(200);

    const auto range_ok = TimeRange::create(start, dur);
    ASSERT_TRUE(range_ok.has_value());
    EXPECT_EQ(range_ok->start(), start);
    EXPECT_EQ(range_ok->duration(), dur);
    EXPECT_EQ(range_ok->end(), TimePoint::from_ticks(300));

    const auto range_neg = TimeRange::create(start, Duration::from_ticks(-1));
    ASSERT_FALSE(range_neg.has_value());
    EXPECT_EQ(range_neg.error().code(), ErrorCode::InvalidArgument);

    const auto range_from_se = TimeRange::from_start_end(start, TimePoint::from_ticks(300));
    ASSERT_TRUE(range_from_se.has_value());
    EXPECT_EQ(range_from_se->duration(), dur);

    const auto range_invalid_se = TimeRange::from_start_end(start, TimePoint::from_ticks(50));
    ASSERT_FALSE(range_invalid_se.has_value());
    EXPECT_EQ(range_invalid_se.error().code(), ErrorCode::InvalidArgument);
}

TEST(TimeTest, TimeRangeContainsPoint) {
    const auto r = TimeRange::create(TimePoint::from_ticks(100), Duration::from_ticks(50)).value();

    EXPECT_FALSE(r.contains(TimePoint::from_ticks(99)));
    EXPECT_TRUE(r.contains(TimePoint::from_ticks(100)));  // start inclusive
    EXPECT_TRUE(r.contains(TimePoint::from_ticks(125)));
    EXPECT_TRUE(r.contains(TimePoint::from_ticks(149)));
    EXPECT_FALSE(r.contains(TimePoint::from_ticks(150))); // end exclusive
    EXPECT_FALSE(r.contains(TimePoint::from_ticks(151)));
}

TEST(TimeTest, TimeRangeContainsRange) {
    const auto outer = TimeRange::create(TimePoint::from_ticks(100), Duration::from_ticks(100)).value(); // [100, 200)

    const auto inner = TimeRange::create(TimePoint::from_ticks(120), Duration::from_ticks(50)).value();  // [120, 170)
    EXPECT_TRUE(outer.contains(inner));

    const auto same = TimeRange::create(TimePoint::from_ticks(100), Duration::from_ticks(100)).value();
    EXPECT_TRUE(outer.contains(same));

    const auto left_overlap = TimeRange::create(TimePoint::from_ticks(90), Duration::from_ticks(30)).value();
    EXPECT_FALSE(outer.contains(left_overlap));

    const auto right_overlap = TimeRange::create(TimePoint::from_ticks(180), Duration::from_ticks(30)).value();
    EXPECT_FALSE(outer.contains(right_overlap));
}

TEST(TimeTest, TimeRangeOverlapsTouchingRangesDoNotOverlap) {
    const auto r1 = TimeRange::create(TimePoint::from_ticks(0), Duration::from_ticks(100)).value();   // [0, 100)
    const auto r2 = TimeRange::create(TimePoint::from_ticks(100), Duration::from_ticks(100)).value(); // [100, 200)
    const auto r3 = TimeRange::create(TimePoint::from_ticks(99), Duration::from_ticks(100)).value();  // [99, 199)
    const auto r_disjoint = TimeRange::create(TimePoint::from_ticks(200), Duration::from_ticks(50)).value();

    // Touching ranges must NOT overlap
    EXPECT_FALSE(r1.overlaps(r2));
    EXPECT_FALSE(r2.overlaps(r1));

    // Overlapping by 1 tick
    EXPECT_TRUE(r1.overlaps(r3));
    EXPECT_TRUE(r3.overlaps(r1));

    // Disjoint
    EXPECT_FALSE(r1.overlaps(r_disjoint));
}

TEST(TimeTest, TimeRangeIntersection) {
    const auto r1 = TimeRange::create(TimePoint::from_ticks(0), Duration::from_ticks(100)).value();   // [0, 100)
    const auto r2 = TimeRange::create(TimePoint::from_ticks(50), Duration::from_ticks(100)).value();  // [50, 150)
    const auto r_touching = TimeRange::create(TimePoint::from_ticks(100), Duration::from_ticks(50)).value();
    const auto r_disjoint = TimeRange::create(TimePoint::from_ticks(150), Duration::from_ticks(50)).value();

    const auto inter12 = r1.intersection(r2);
    ASSERT_TRUE(inter12.has_value());
    EXPECT_EQ(inter12->start(), TimePoint::from_ticks(50));
    EXPECT_EQ(inter12->duration(), Duration::from_ticks(50));
    EXPECT_EQ(inter12->end(), TimePoint::from_ticks(100));

    EXPECT_FALSE(r1.intersection(r_touching).has_value());
    EXPECT_FALSE(r1.intersection(r_disjoint).has_value());
}

}  // namespace
}  // namespace nxtcut::core
