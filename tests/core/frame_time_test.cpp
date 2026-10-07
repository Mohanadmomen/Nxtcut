#include <nxtcut/core/frame_rate.hpp>
#include <nxtcut/core/frame_time.hpp>

#include <gtest/gtest.h>

#include <array>
#include <vector>

#include "compile_checks.hpp"

namespace nxtcut::core {
namespace {

using test::CanAdd;
using test::CanSubtract;

static_assert(!CanAdd<FrameIndex, FrameIndex>);
static_assert(CanAdd<FrameIndex, std::int64_t>);
static_assert(CanAdd<std::int64_t, FrameIndex>);
static_assert(CanSubtract<FrameIndex, std::int64_t>);
static_assert(CanSubtract<FrameIndex, FrameIndex>);
static_assert(!std::is_convertible_v<FrameIndex, std::int64_t>,
              "FrameIndex must not convert implicitly to int64");
static_assert(!std::is_convertible_v<std::int64_t, FrameIndex>,
              "int64 must not convert implicitly to FrameIndex");

TEST(FrameTimeTest, FrameIndexArithmetic) {
    FrameIndex idx(100);
    EXPECT_EQ(idx.value(), 100);

    EXPECT_EQ((idx + 25).value(), 125);
    EXPECT_EQ((25 + idx).value(), 125);
    EXPECT_EQ((idx - 30).value(), 70);
    EXPECT_EQ(idx - FrameIndex(40), 60);

    idx += 50;
    EXPECT_EQ(idx.value(), 150);
    idx -= 20;
    EXPECT_EQ(idx.value(), 130);

    EXPECT_EQ((++idx).value(), 131);
    EXPECT_EQ((idx++).value(), 131);
    EXPECT_EQ(idx.value(), 132);

    EXPECT_EQ((--idx).value(), 131);
    EXPECT_EQ((idx--).value(), 131);
    EXPECT_EQ(idx.value(), 130);
}

TEST(FrameTimeTest, FrameDurationExactness) {
    using namespace frame_rates;

    // 24 fps: 705,600,000 / 24 = 29,400,000 ticks
    const auto d24 = frame_duration(k24);
    ASSERT_TRUE(d24.has_value());
    EXPECT_EQ(d24->ticks(), 29'400'000);

    // 25 fps: 705,600,000 / 25 = 28,224,000 ticks
    const auto d25 = frame_duration(k25);
    ASSERT_TRUE(d25.has_value());
    EXPECT_EQ(d25->ticks(), 28'224'000);

    // 29.97 fps (30000/1001): 705,600,000 * 1001 / 30000 = 23,543,520 ticks
    const auto d2997 = frame_duration(k29_97);
    ASSERT_TRUE(d2997.has_value());
    EXPECT_EQ(d2997->ticks(), 23'543'520);

    // 59.94 fps (60000/1001): 705,600,000 * 1001 / 60000 = 11,771,760 ticks
    const auto d5994 = frame_duration(k59_94);
    ASSERT_TRUE(d5994.has_value());
    EXPECT_EQ(d5994->ticks(), 11'771'760);
}

TEST(FrameTimeTest, SnapToFrame) {
    const FrameRate rate = frame_rates::k24;  // 29,400,000 ticks per frame

    // Exact frame 1
    const TimePoint tp_frame1 = TimePoint::from_ticks(29'400'000);
    const auto snap_exact = snap_to_frame(tp_frame1, rate, RoundingMode::Nearest);
    ASSERT_TRUE(snap_exact.has_value());
    EXPECT_EQ(snap_exact->ticks(), 29'400'000);

    // Slight offset into frame 1: 29,400,100 ticks
    const TimePoint tp_slight = TimePoint::from_ticks(29'400'100);
    EXPECT_EQ(snap_to_frame(tp_slight, rate, RoundingMode::Floor)->ticks(), 29'400'000);
    EXPECT_EQ(snap_to_frame(tp_slight, rate, RoundingMode::Nearest)->ticks(), 29'400'000);
    EXPECT_EQ(snap_to_frame(tp_slight, rate, RoundingMode::Ceil)->ticks(), 58'800'000);
}

TEST(FrameTimeTest, AudioSampleConversions) {
    using namespace sample_rates;

    // 48 kHz: 48,000 samples = exactly 1 second (705,600,000 ticks)
    const auto tp48 = samples_to_time(48000, k48000);
    ASSERT_TRUE(tp48.has_value());
    EXPECT_EQ(tp48->ticks(), 705'600'000);

    const auto s48_back = time_to_samples(*tp48, k48000, RoundingMode::Nearest);
    ASSERT_TRUE(s48_back.has_value());
    EXPECT_EQ(*s48_back, 48000);

    // 44.1 kHz: 44,100 samples = exactly 1 second
    const auto tp441 = samples_to_time(44100, k44100);
    ASSERT_TRUE(tp441.has_value());
    EXPECT_EQ(tp441->ticks(), 705'600'000);

    const auto s441_back = time_to_samples(*tp441, k44100, RoundingMode::Nearest);
    ASSERT_TRUE(s441_back.has_value());
    EXPECT_EQ(*s441_back, 44100);

    // Round-trip 10,000 consecutive samples
    for (std::int64_t sample = 0; sample <= 10000; ++sample) {
        const auto time = samples_to_time(sample, k48000).value();
        const auto roundtrip = time_to_samples(time, k48000, RoundingMode::Nearest).value();
        EXPECT_EQ(roundtrip, sample);
    }
}

TEST(FrameTimeTest, FrameTimeToFrameRoundTripInvariant) {
    using namespace frame_rates;
    const std::array<FrameRate, 8> rates = {k23_976, k24, k25, k29_97, k30, k50, k59_94, k60};

    std::vector<std::int64_t> test_frames;
    test_frames.reserve(500);

    // Boundary ends [-1,000,000 .. -999,950] and [999,950 .. 1,000,000]
    for (std::int64_t f = -1'000'000; f <= -999'950; ++f) {
        test_frames.push_back(f);
    }
    for (std::int64_t f = 999'950; f <= 1'000'000; ++f) {
        test_frames.push_back(f);
    }

    // Near zero [-200 .. 200]
    for (std::int64_t f = -200; f <= 200; ++f) {
        test_frames.push_back(f);
    }

    // Stepped throughout the range
    for (std::int64_t f = -900'000; f <= 900'000; f += 50'000) {
        test_frames.push_back(f);
    }

    for (const auto& rate : rates) {
        for (const std::int64_t f : test_frames) {
            const auto tp = frame_to_time(FrameIndex(f), rate);
            ASSERT_TRUE(tp.has_value()) << "Failed frame_to_time for frame " << f;

            const auto recovered = time_to_frame(*tp, rate, RoundingMode::Nearest);
            ASSERT_TRUE(recovered.has_value()) << "Failed time_to_frame for frame " << f;
            EXPECT_EQ(recovered->value(), f)
                << "Mismatch for rate " << rate.to_string() << " at frame " << f;
        }
    }
}

}  // namespace
}  // namespace nxtcut::core
