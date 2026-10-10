#include <nxtcut/keyframes/keyframe_track.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <vector>

#include <nxtcut_test/assertions.hpp>

namespace nxtcut::keyframes {
namespace {

TEST(KeyframeTrackTest, CreateValidation) {
    // Accepts empty
    const auto empty_res = KeyframeTrack<double>::create({});
    EXPECT_TRUE(test::is_ok(empty_res));
    EXPECT_EQ(empty_res->size(), 0U);
    EXPECT_TRUE(empty_res->empty());

    // Rejects non-finite values
    const double nan_val = std::numeric_limits<double>::quiet_NaN();
    const double inf_val = std::numeric_limits<double>::infinity();
    EXPECT_TRUE(
        test::is_error(KeyframeTrack<double>::create({
                           {core::TimePoint::from_ticks(0), nan_val, Interpolation::linear()},
                       }),
                       core::ErrorCode::InvalidArgument));
    EXPECT_TRUE(
        test::is_error(KeyframeTrack<double>::create({
                           {core::TimePoint::from_ticks(0), inf_val, Interpolation::linear()},
                       }),
                       core::ErrorCode::InvalidArgument));

    // Rejects time out of range (+/- 2^62)
    const std::int64_t too_large = kMaxKeyframeTicks + 1;
    const std::int64_t too_small = -kMaxKeyframeTicks - 1;
    EXPECT_TRUE(
        test::is_error(KeyframeTrack<double>::create({
                           {core::TimePoint::from_ticks(too_large), 1.0, Interpolation::linear()},
                       }),
                       core::ErrorCode::InvalidArgument));
    EXPECT_TRUE(
        test::is_error(KeyframeTrack<double>::create({
                           {core::TimePoint::from_ticks(too_small), 1.0, Interpolation::linear()},
                       }),
                       core::ErrorCode::InvalidArgument));

    // Rejects unsorted times
    EXPECT_TRUE(test::is_error(KeyframeTrack<double>::create({
                                   {core::TimePoint::from_ticks(100), 1.0, Interpolation::linear()},
                                   {core::TimePoint::from_ticks(50), 2.0, Interpolation::linear()},
                               }),
                               core::ErrorCode::InvalidArgument));

    // Rejects duplicate times
    EXPECT_TRUE(test::is_error(KeyframeTrack<double>::create({
                                   {core::TimePoint::from_ticks(100), 1.0, Interpolation::linear()},
                                   {core::TimePoint::from_ticks(100), 2.0, Interpolation::linear()},
                               }),
                               core::ErrorCode::InvalidArgument));
}

TEST(KeyframeTrackTest, ValueAtEmptyAndSingleKeyframe) {
    KeyframeTrack<double> empty_track;
    EXPECT_EQ(empty_track.value_at(core::TimePoint::from_ticks(0)), 0.0);
    EXPECT_EQ(empty_track.value_at(core::TimePoint::from_ticks(100)), 0.0);

    const auto single_res = KeyframeTrack<double>::create({
        {core::TimePoint::from_ticks(100), 42.0, Interpolation::linear()},
    });
    ASSERT_TRUE(single_res.has_value());
    const auto& track = *single_res;

    // Single keyframe returns its value everywhere
    EXPECT_EQ(track.value_at(core::TimePoint::from_ticks(0)), 42.0);
    EXPECT_EQ(track.value_at(core::TimePoint::from_ticks(100)), 42.0);
    EXPECT_EQ(track.value_at(core::TimePoint::from_ticks(200)), 42.0);
}

TEST(KeyframeTrackTest, ValueAtLinearTwoKeys) {
    const auto res = KeyframeTrack<double>::create({
        {core::TimePoint::from_ticks(0), 0.0, Interpolation::linear()},
        {core::TimePoint::from_ticks(100), 10.0, Interpolation::linear()},
    });
    ASSERT_TRUE(res.has_value());
    const auto& track = *res;

    EXPECT_EQ(track.value_at(core::TimePoint::from_ticks(0)), 0.0);
    EXPECT_EQ(track.value_at(core::TimePoint::from_ticks(25)), 2.5);
    EXPECT_EQ(track.value_at(core::TimePoint::from_ticks(50)), 5.0);
    EXPECT_EQ(track.value_at(core::TimePoint::from_ticks(100)), 10.0);

    // Before first and after last clamp to boundary values
    EXPECT_EQ(track.value_at(core::TimePoint::from_ticks(-50)), 0.0);
    EXPECT_EQ(track.value_at(core::TimePoint::from_ticks(150)), 10.0);
}

TEST(KeyframeTrackTest, ValueAtHoldSegment) {
    const auto res = KeyframeTrack<double>::create({
        {core::TimePoint::from_ticks(0), 0.0, Interpolation::hold()},
        {core::TimePoint::from_ticks(100), 10.0, Interpolation::linear()},
    });
    ASSERT_TRUE(res.has_value());
    const auto& track = *res;

    EXPECT_EQ(track.value_at(core::TimePoint::from_ticks(0)), 0.0);
    EXPECT_EQ(track.value_at(core::TimePoint::from_ticks(50)), 0.0);
    EXPECT_EQ(track.value_at(core::TimePoint::from_ticks(99)), 0.0);
    EXPECT_EQ(track.value_at(core::TimePoint::from_ticks(100)), 10.0);
}

TEST(KeyframeTrackTest, ValueAtEasing) {
    const auto res = KeyframeTrack<double>::create({
        {core::TimePoint::from_ticks(0), 0.0, Interpolation::easing(EasingKind::EaseInQuad)},
        {core::TimePoint::from_ticks(100), 10.0, Interpolation::linear()},
    });
    ASSERT_TRUE(res.has_value());
    const auto& track = *res;

    // InQuad(0.5) is 0.25 -> 0.25 * 10.0 = 2.5
    EXPECT_NEAR(track.value_at(core::TimePoint::from_ticks(50)), 2.5, 1e-9);
}

TEST(KeyframeTrackTest, ValueAtBezierLinearEquivalent) {
    const auto bez = Interpolation::bezier(1.0 / 3.0, 1.0 / 3.0, 2.0 / 3.0, 2.0 / 3.0).value();
    const auto res = KeyframeTrack<double>::create({
        {core::TimePoint::from_ticks(0), 0.0, bez},
        {core::TimePoint::from_ticks(100), 10.0, Interpolation::linear()},
    });
    ASSERT_TRUE(res.has_value());
    const auto& track = *res;

    EXPECT_NEAR(track.value_at(core::TimePoint::from_ticks(50)), 5.0, 1e-9);
}

TEST(KeyframeTrackTest, ValueAtMultiSegment) {
    // {0: 0 linear, 100: 10 hold, 200: 20 linear, 300: 0 linear}
    const auto res = KeyframeTrack<double>::create({
        {core::TimePoint::from_ticks(0), 0.0, Interpolation::linear()},
        {core::TimePoint::from_ticks(100), 10.0, Interpolation::hold()},
        {core::TimePoint::from_ticks(200), 20.0, Interpolation::linear()},
        {core::TimePoint::from_ticks(300), 0.0, Interpolation::linear()},
    });
    ASSERT_TRUE(res.has_value());
    const auto& track = *res;

    EXPECT_DOUBLE_EQ(track.value_at(core::TimePoint::from_ticks(50)), 5.0);
    EXPECT_DOUBLE_EQ(track.value_at(core::TimePoint::from_ticks(150)), 10.0);
    EXPECT_DOUBLE_EQ(track.value_at(core::TimePoint::from_ticks(200)), 20.0);
    EXPECT_DOUBLE_EQ(track.value_at(core::TimePoint::from_ticks(250)), 10.0);
    EXPECT_DOUBLE_EQ(track.value_at(core::TimePoint::from_ticks(300)), 0.0);
    EXPECT_DOUBLE_EQ(track.value_at(core::TimePoint::from_ticks(400)), 0.0);
}

TEST(KeyframeTrackTest, LastKeyframeInterpolationIgnored) {
    // Last keyframe has hold, but after t=100 it returns 10.0
    const auto res = KeyframeTrack<double>::create({
        {core::TimePoint::from_ticks(0), 0.0, Interpolation::linear()},
        {core::TimePoint::from_ticks(100), 10.0, Interpolation::hold()},
    });
    ASSERT_TRUE(res.has_value());
    const auto& track = *res;

    EXPECT_EQ(track.value_at(core::TimePoint::from_ticks(150)), 10.0);
}

TEST(KeyframeTrackTest, SetOperations) {
    KeyframeTrack<double> track;

    // Insert first
    const auto idx0 = track.set({core::TimePoint::from_ticks(100), 10.0, Interpolation::linear()});
    ASSERT_TRUE(idx0.has_value());
    EXPECT_EQ(*idx0, 0U);
    EXPECT_EQ(track.size(), 1U);

    // Insert before
    const auto idx_before =
        track.set({core::TimePoint::from_ticks(0), 0.0, Interpolation::linear()});
    ASSERT_TRUE(idx_before.has_value());
    EXPECT_EQ(*idx_before, 0U);
    EXPECT_EQ(track.size(), 2U);

    // Insert after
    const auto idx_after =
        track.set({core::TimePoint::from_ticks(300), 30.0, Interpolation::linear()});
    ASSERT_TRUE(idx_after.has_value());
    EXPECT_EQ(*idx_after, 2U);
    EXPECT_EQ(track.size(), 3U);

    // Insert between
    const auto idx_mid =
        track.set({core::TimePoint::from_ticks(200), 20.0, Interpolation::linear()});
    ASSERT_TRUE(idx_mid.has_value());
    EXPECT_EQ(*idx_mid, 2U);
    EXPECT_EQ(track.size(), 4U);

    // Replace same time
    const auto idx_replace =
        track.set({core::TimePoint::from_ticks(200), 25.0, Interpolation::hold()});
    ASSERT_TRUE(idx_replace.has_value());
    EXPECT_EQ(*idx_replace, 2U);
    EXPECT_EQ(track.size(), 4U);
    EXPECT_EQ(track.keys()[2].value, 25.0);
    EXPECT_EQ(track.keys()[2].interpolation.kind(), InterpolationKind::Hold);

    // Invalid value rejected and track unchanged
    const double nan_val = std::numeric_limits<double>::quiet_NaN();
    const auto err_res =
        track.set({core::TimePoint::from_ticks(250), nan_val, Interpolation::linear()});
    EXPECT_TRUE(test::is_error(err_res, core::ErrorCode::InvalidArgument));
    EXPECT_EQ(track.size(), 4U);
}

TEST(KeyframeTrackTest, FindAndRemoveAt) {
    auto track = KeyframeTrack<double>::create(
                     {
                         {core::TimePoint::from_ticks(10), 1.0, Interpolation::linear()},
                         {core::TimePoint::from_ticks(20), 2.0, Interpolation::linear()},
                         {core::TimePoint::from_ticks(30), 3.0, Interpolation::linear()},
                     })
                     .value();

    // Find present and absent
    const auto f_present = track.find(core::TimePoint::from_ticks(20));
    ASSERT_TRUE(f_present.has_value());
    EXPECT_EQ(*f_present, 1U);

    const auto f_absent = track.find(core::TimePoint::from_ticks(25));
    EXPECT_FALSE(f_absent.has_value());

    // Remove present and absent
    EXPECT_FALSE(track.remove_at(core::TimePoint::from_ticks(99)));
    EXPECT_EQ(track.size(), 3U);

    EXPECT_TRUE(track.remove_at(core::TimePoint::from_ticks(20)));
    EXPECT_EQ(track.size(), 2U);
    EXPECT_FALSE(track.find(core::TimePoint::from_ticks(20)).has_value());
}

}  // namespace
}  // namespace nxtcut::keyframes
