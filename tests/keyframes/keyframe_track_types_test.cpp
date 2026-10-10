#include <nxtcut/core/color.hpp>
#include <nxtcut/core/geometry.hpp>
#include <nxtcut/keyframes/keyframe_track.hpp>

#include <gtest/gtest.h>

#include <limits>

#include <nxtcut_test/assertions.hpp>

namespace nxtcut::keyframes {
namespace {

TEST(KeyframeTrackTypesTest, ColorTrackEvaluation) {
    const core::Color c1{0.0f, 0.2f, 0.4f, 1.0f};
    const core::Color c2{1.0f, 0.6f, 0.8f, 1.0f};

    const auto track = KeyframeTrack<core::Color>::create(
                           {
                               {core::TimePoint::from_ticks(0), c1, Interpolation::linear()},
                               {core::TimePoint::from_ticks(100), c2, Interpolation::linear()},
                           })
                           .value();

    // Endpoints exact
    const auto at_0 = track.value_at(core::TimePoint::from_ticks(0));
    EXPECT_TRUE(AnimatableTraits<core::Color>::identical(at_0, c1));

    const auto at_100 = track.value_at(core::TimePoint::from_ticks(100));
    EXPECT_TRUE(AnimatableTraits<core::Color>::identical(at_100, c2));

    // Midpoint
    const auto at_50 = track.value_at(core::TimePoint::from_ticks(50));
    EXPECT_FLOAT_EQ(at_50.r, 0.5f);
    EXPECT_FLOAT_EQ(at_50.g, 0.4f);
    EXPECT_FLOAT_EQ(at_50.b, 0.6f);
    EXPECT_FLOAT_EQ(at_50.a, 1.0f);

    // Rejects non-finite Color
    core::Color nan_c = c1;
    nan_c.g = std::numeric_limits<float>::quiet_NaN();
    EXPECT_TRUE(test::is_error(KeyframeTrack<core::Color>::create({
                                   {core::TimePoint::from_ticks(0), nan_c, Interpolation::linear()},
                               }),
                               core::ErrorCode::InvalidArgument));
}

TEST(KeyframeTrackTypesTest, PointTrackEvaluation) {
    const core::Point<double> p1{10.0, 20.0};
    const core::Point<double> p2{50.0, 80.0};

    const auto track = KeyframeTrack<core::Point<double>>::create(
                           {
                               {core::TimePoint::from_ticks(0), p1, Interpolation::linear()},
                               {core::TimePoint::from_ticks(100), p2, Interpolation::linear()},
                           })
                           .value();

    // Endpoints exact
    const auto at_0 = track.value_at(core::TimePoint::from_ticks(0));
    EXPECT_TRUE(AnimatableTraits<core::Point<double>>::identical(at_0, p1));

    const auto at_100 = track.value_at(core::TimePoint::from_ticks(100));
    EXPECT_TRUE(AnimatableTraits<core::Point<double>>::identical(at_100, p2));

    // Midpoint
    const auto at_50 = track.value_at(core::TimePoint::from_ticks(50));
    EXPECT_DOUBLE_EQ(at_50.x, 30.0);
    EXPECT_DOUBLE_EQ(at_50.y, 50.0);

    // Rejects non-finite Point
    core::Point<double> inf_p{std::numeric_limits<double>::infinity(), 0.0};
    EXPECT_TRUE(test::is_error(KeyframeTrack<core::Point<double>>::create({
                                   {core::TimePoint::from_ticks(0), inf_p, Interpolation::linear()},
                               }),
                               core::ErrorCode::InvalidArgument));
}

}  // namespace
}  // namespace nxtcut::keyframes
