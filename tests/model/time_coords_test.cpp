#include <nxtcut/model/time_coords.hpp>

#include <gtest/gtest.h>

#include "compile_checks.hpp"

namespace nxtcut::model {
namespace {

using test::CanAdd;
using test::CanConstruct;
using test::CanConvert;
using test::CanEqual;
using test::CanOrder;
using test::CanSubtract;

// Static assertions proving no conversion, no construction, and no arithmetic between the three
// types
static_assert(!CanConvert<TimelineTime, ClipTime>);
static_assert(!CanConvert<ClipTime, TimelineTime>);
static_assert(!CanConvert<TimelineTime, SourceTime>);
static_assert(!CanConvert<SourceTime, TimelineTime>);
static_assert(!CanConvert<ClipTime, SourceTime>);
static_assert(!CanConvert<SourceTime, ClipTime>);

static_assert(!CanConstruct<TimelineTime, ClipTime>);
static_assert(!CanConstruct<ClipTime, TimelineTime>);
static_assert(!CanConstruct<TimelineTime, SourceTime>);
static_assert(!CanConstruct<SourceTime, TimelineTime>);
static_assert(!CanConstruct<ClipTime, SourceTime>);
static_assert(!CanConstruct<SourceTime, ClipTime>);

static_assert(!CanAdd<TimelineTime, ClipTime>);
static_assert(!CanAdd<ClipTime, TimelineTime>);
static_assert(!CanAdd<TimelineTime, SourceTime>);
static_assert(!CanAdd<SourceTime, TimelineTime>);
static_assert(!CanAdd<ClipTime, SourceTime>);
static_assert(!CanAdd<SourceTime, ClipTime>);

static_assert(!CanSubtract<TimelineTime, ClipTime>);
static_assert(!CanSubtract<ClipTime, TimelineTime>);
static_assert(!CanSubtract<TimelineTime, SourceTime>);
static_assert(!CanSubtract<SourceTime, TimelineTime>);
static_assert(!CanSubtract<ClipTime, SourceTime>);
static_assert(!CanSubtract<SourceTime, ClipTime>);

static_assert(CanEqual<TimelineTime, TimelineTime>);
static_assert(CanOrder<TimelineTime, TimelineTime>);
static_assert(CanEqual<ClipTime, ClipTime>);
static_assert(CanOrder<ClipTime, ClipTime>);
static_assert(CanEqual<SourceTime, SourceTime>);
static_assert(CanOrder<SourceTime, SourceTime>);

TEST(TimeCoordsTest, TimelineTimeBasics) {
    const TimelineTime default_t;
    EXPECT_EQ(default_t.ticks(), 0);
    EXPECT_EQ(default_t.to_core(), core::TimePoint::zero());

    const TimelineTime t100 = TimelineTime::from_ticks(100);
    EXPECT_EQ(t100.ticks(), 100);
    EXPECT_EQ(t100.to_core(), core::TimePoint::from_ticks(100));

    const TimelineTime t200 = TimelineTime::from_ticks(200);
    EXPECT_LT(t100, t200);
    EXPECT_LE(t100, t200);
    EXPECT_GT(t200, t100);
    EXPECT_GE(t200, t100);
    EXPECT_NE(t100, t200);
    EXPECT_EQ(t100, TimelineTime::from_ticks(100));
}

TEST(TimeCoordsTest, ClipTimeBasics) {
    const ClipTime default_c;
    EXPECT_EQ(default_c.ticks(), 0);
    EXPECT_EQ(default_c.to_core(), core::TimePoint::zero());

    const ClipTime c50 = ClipTime::from_ticks(50);
    EXPECT_EQ(c50.ticks(), 50);

    const ClipTime c75 = ClipTime::from_ticks(75);
    EXPECT_LT(c50, c75);
    EXPECT_EQ(c50, ClipTime::from_ticks(50));
}

TEST(TimeCoordsTest, SourceTimeBasics) {
    const SourceTime default_s;
    EXPECT_EQ(default_s.ticks(), 0);
    EXPECT_EQ(default_s.to_core(), core::TimePoint::zero());

    const SourceTime s1000 = SourceTime::from_ticks(1000);
    EXPECT_EQ(s1000.ticks(), 1000);

    const SourceTime s2000 = SourceTime::from_ticks(2000);
    EXPECT_LT(s1000, s2000);
    EXPECT_EQ(s1000, SourceTime::from_ticks(1000));
}

}  // namespace
}  // namespace nxtcut::model
