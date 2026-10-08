#include <nxtcut/model/sequence.hpp>

#include <gtest/gtest.h>

#include <limits>

namespace nxtcut::model {
namespace {

constexpr std::int64_t T = core::kTicksPerSecond;

TEST(SequenceTest, Defaults) {
    Sequence seq;
    EXPECT_EQ(seq.frame_rate, core::frame_rates::k30);
    EXPECT_EQ(seq.canvas.width, 1920);
    EXPECT_EQ(seq.canvas.height, 1080);
    EXPECT_EQ(seq.sample_rate, core::sample_rates::k48000);
    EXPECT_TRUE(seq.background.approx_equal(core::Color{0.0f, 0.0f, 0.0f, 1.0f}));
    EXPECT_TRUE(seq.tracks.empty());
    EXPECT_TRUE(seq.markers.empty());
}

TEST(SequenceTest, DurationEmptySequenceIsZero) {
    Sequence seq;
    const auto dur = sequence_duration(seq);
    ASSERT_TRUE(dur.has_value());
    EXPECT_EQ(dur->ticks(), 0);
}

TEST(SequenceTest, DurationMultipleTracksTakesLatestEnd) {
    Sequence seq;

    Track t1;
    Clip c1;
    c1.start = TimelineTime::from_ticks(0);
    c1.duration = core::Duration::from_ticks(5 * T);  // ends at 5s
    t1.clips.push_back(c1);

    Track t2;
    Clip c2;
    c2.start = TimelineTime::from_ticks(2 * T);
    c2.duration = core::Duration::from_ticks(8 * T);  // ends at 10s
    t2.clips.push_back(c2);

    Clip c3;
    c3.start = TimelineTime::from_ticks(1 * T);
    c3.duration = core::Duration::from_ticks(6 * T);  // ends at 7s
    t2.clips.push_back(c3);

    seq.tracks.push_back(t1);
    seq.tracks.push_back(t2);

    const auto dur = sequence_duration(seq);
    ASSERT_TRUE(dur.has_value());
    EXPECT_EQ(dur->ticks(), 10 * T);
}

TEST(SequenceTest, DurationPropagatesOverflow) {
    Sequence seq;
    Track t;
    Clip c;
    c.start = TimelineTime::from_ticks(std::numeric_limits<std::int64_t>::max() - 10);
    c.duration = core::Duration::from_ticks(20);
    t.clips.push_back(c);
    seq.tracks.push_back(t);

    const auto dur = sequence_duration(seq);
    EXPECT_FALSE(dur.has_value());
    EXPECT_EQ(dur.error().code(), core::ErrorCode::Overflow);
}

}  // namespace
}  // namespace nxtcut::model
