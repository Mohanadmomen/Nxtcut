#include <nxtcut/model/clip.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <random>

namespace nxtcut::model {
namespace {

constexpr std::int64_t T = core::kTicksPerSecond;

TEST(ClipTest, ContentDefaultsAndKindOf) {
    core::UuidGenerator gen(1234ULL);
    const MediaId mid = generate_id<MediaId>(gen);
    const SequenceId sid = generate_id<SequenceId>(gen);

    VideoContent v{mid};
    EXPECT_EQ(kind_of(v), ClipKind::Video);

    AudioContent a{mid};
    EXPECT_EQ(kind_of(a), ClipKind::Audio);
    EXPECT_DOUBLE_EQ(a.volume.constant_value(), 1.0);
    EXPECT_EQ(a.fade_in.ticks(), 0);
    EXPECT_EQ(a.fade_out.ticks(), 0);

    ImageContent img{mid};
    EXPECT_EQ(kind_of(img), ClipKind::Image);

    TextContent txt;
    EXPECT_EQ(kind_of(txt), ClipKind::Text);
    EXPECT_DOUBLE_EQ(txt.font_size_px.constant_value(), 48.0);
    EXPECT_TRUE(txt.color.constant_value().approx_equal(core::Color{1.0f, 1.0f, 1.0f, 1.0f}));

    CompoundContent comp{sid};
    EXPECT_EQ(kind_of(comp), ClipKind::Compound);
}

TEST(ClipTest, ClipKindToString) {
    EXPECT_EQ(to_string(ClipKind::Video), "Video");
    EXPECT_EQ(to_string(ClipKind::Audio), "Audio");
    EXPECT_EQ(to_string(ClipKind::Image), "Image");
    EXPECT_EQ(to_string(ClipKind::Text), "Text");
    EXPECT_EQ(to_string(ClipKind::Compound), "Compound");
}

// REQUIRED Vector V1:
// start 10 s, duration 5 s, source_in 2 s, speed 1/1:
// timeline 12 s gives clip 2 s and source 4 s.
// timeline 10 s gives clip 0 and source 2 s.
// timeline 15 s gives OutOfRange (half-open).
// timeline 15 s minus 1 tick gives clip 5 s minus 1 tick.
// timeline 9 s gives OutOfRange.
TEST(ClipTest, VectorV1) {
    Clip clip;
    clip.start = TimelineTime::from_ticks(10 * T);
    clip.duration = core::Duration::from_ticks(5 * T);
    clip.source_in = SourceTime::from_ticks(2 * T);
    clip.speed = Speed::normal();

    // timeline 12 s
    const auto c12 = timeline_to_clip(clip, TimelineTime::from_ticks(12 * T));
    ASSERT_TRUE(c12.has_value());
    EXPECT_EQ(c12->ticks(), 2 * T);

    const auto s12 = timeline_to_source(clip, TimelineTime::from_ticks(12 * T));
    ASSERT_TRUE(s12.has_value());
    EXPECT_EQ(s12->ticks(), 4 * T);

    // timeline 10 s
    const auto c10 = timeline_to_clip(clip, TimelineTime::from_ticks(10 * T));
    ASSERT_TRUE(c10.has_value());
    EXPECT_EQ(c10->ticks(), 0);

    const auto s10 = timeline_to_source(clip, TimelineTime::from_ticks(10 * T));
    ASSERT_TRUE(s10.has_value());
    EXPECT_EQ(s10->ticks(), 2 * T);

    // timeline 15 s gives OutOfRange (half-open)
    const auto c15 = timeline_to_clip(clip, TimelineTime::from_ticks(15 * T));
    EXPECT_FALSE(c15.has_value());
    EXPECT_EQ(c15.error().code(), core::ErrorCode::OutOfRange);

    const auto s15 = timeline_to_source(clip, TimelineTime::from_ticks(15 * T));
    EXPECT_FALSE(s15.has_value());
    EXPECT_EQ(s15.error().code(), core::ErrorCode::OutOfRange);

    // timeline 15 s minus 1 tick gives clip 5 s minus 1 tick
    const auto c15_minus = timeline_to_clip(clip, TimelineTime::from_ticks(15 * T - 1));
    ASSERT_TRUE(c15_minus.has_value());
    EXPECT_EQ(c15_minus->ticks(), 5 * T - 1);

    // timeline 9 s gives OutOfRange
    const auto c9 = timeline_to_clip(clip, TimelineTime::from_ticks(9 * T));
    EXPECT_FALSE(c9.has_value());
    EXPECT_EQ(c9.error().code(), core::ErrorCode::OutOfRange);
}

// REQUIRED Vector V2:
// start 0, duration 5 s, source_in 0, speed 2/1:
// timeline 1 s gives source 2 s.
// clip_to_source(5 s) gives 10 s (end inclusive).
// source_to_clip(10 s) gives 5 s.
// clip_to_source(5 s + 1 tick) gives OutOfRange.
TEST(ClipTest, VectorV2) {
    Clip clip;
    clip.start = TimelineTime::zero();
    clip.duration = core::Duration::from_ticks(5 * T);
    clip.source_in = SourceTime::zero();
    clip.speed = Speed::create(2, 1).value();

    const auto s1 = timeline_to_source(clip, TimelineTime::from_ticks(1 * T));
    ASSERT_TRUE(s1.has_value());
    EXPECT_EQ(s1->ticks(), 2 * T);

    const auto s5 = clip_to_source(clip, ClipTime::from_ticks(5 * T));
    ASSERT_TRUE(s5.has_value());
    EXPECT_EQ(s5->ticks(), 10 * T);

    const auto c10 = source_to_clip(clip, SourceTime::from_ticks(10 * T));
    ASSERT_TRUE(c10.has_value());
    EXPECT_EQ(c10->ticks(), 5 * T);

    const auto s_over = clip_to_source(clip, ClipTime::from_ticks(5 * T + 1));
    EXPECT_FALSE(s_over.has_value());
    EXPECT_EQ(s_over.error().code(), core::ErrorCode::OutOfRange);
}

// REQUIRED Vector V3:
// start 0, duration 10 s, source_in 1 s, speed 1/2:
// timeline 3 s gives source 1,764,000,000 ticks (which is 2.5 s).
TEST(ClipTest, VectorV3) {
    Clip clip;
    clip.start = TimelineTime::zero();
    clip.duration = core::Duration::from_ticks(10 * T);
    clip.source_in = SourceTime::from_ticks(1 * T);
    clip.speed = Speed::create(1, 2).value();

    const auto s3 = timeline_to_source(clip, TimelineTime::from_ticks(3 * T));
    ASSERT_TRUE(s3.has_value());
    EXPECT_EQ(s3->ticks(), 1'764'000'000);
}

// REQUIRED Vector V4 (floor):
// source_in 0, duration 10 s:
// speed 3/2 and clip time 5 ticks gives source 7 ticks.
// speed 1/3 gives source 0 for clip times 1 and 2 ticks, and 1 for clip time 3 ticks.
TEST(ClipTest, VectorV4Floor) {
    Clip clip;
    clip.start = TimelineTime::zero();
    clip.duration = core::Duration::from_ticks(10 * T);
    clip.source_in = SourceTime::zero();
    clip.speed = Speed::create(3, 2).value();

    const auto s5 = clip_to_source(clip, ClipTime::from_ticks(5));
    ASSERT_TRUE(s5.has_value());
    EXPECT_EQ(s5->ticks(), 7);

    clip.speed = Speed::create(1, 3).value();
    const auto s_c1 = clip_to_source(clip, ClipTime::from_ticks(1));
    ASSERT_TRUE(s_c1.has_value());
    EXPECT_EQ(s_c1->ticks(), 0);

    const auto s_c2 = clip_to_source(clip, ClipTime::from_ticks(2));
    ASSERT_TRUE(s_c2.has_value());
    EXPECT_EQ(s_c2->ticks(), 0);

    const auto s_c3 = clip_to_source(clip, ClipTime::from_ticks(3));
    ASSERT_TRUE(s_c3.has_value());
    EXPECT_EQ(s_c3->ticks(), 1);
}

// REQUIRED Vector V5 (inverse floor):
// source_in 0, speed 3/2:
// source 7 ticks gives clip 4; source 6 gives 4; source 3 gives 2.
// A source time before source_in gives OutOfRange.
TEST(ClipTest, VectorV5InverseFloor) {
    Clip clip;
    clip.start = TimelineTime::zero();
    clip.duration = core::Duration::from_ticks(10 * T);
    clip.source_in = SourceTime::from_ticks(0);
    clip.speed = Speed::create(3, 2).value();

    const auto c7 = source_to_clip(clip, SourceTime::from_ticks(7));
    ASSERT_TRUE(c7.has_value());
    EXPECT_EQ(c7->ticks(), 4);

    const auto c6 = source_to_clip(clip, SourceTime::from_ticks(6));
    ASSERT_TRUE(c6.has_value());
    EXPECT_EQ(c6->ticks(), 4);

    const auto c3 = source_to_clip(clip, SourceTime::from_ticks(3));
    ASSERT_TRUE(c3.has_value());
    EXPECT_EQ(c3->ticks(), 2);

    clip.source_in = SourceTime::from_ticks(10);
    const auto c_before = source_to_clip(clip, SourceTime::from_ticks(5));
    EXPECT_FALSE(c_before.has_value());
    EXPECT_EQ(c_before.error().code(), core::ErrorCode::OutOfRange);
}

// REQUIRED Vector V6 (overflow):
// source_in = INT64_MAX minus 5 ticks, speed 1/1, duration 100 ticks, clip time 10 ticks:
// clip_to_source gives Overflow. Duration INT64_MAX ticks with speed 2/1: source_span gives
// Overflow. start = INT64_MAX minus 5 ticks with duration 10 ticks: clip_end gives Overflow.
TEST(ClipTest, VectorV6Overflow) {
    Clip clip;
    clip.start = TimelineTime::zero();
    clip.duration = core::Duration::from_ticks(100);
    clip.source_in = SourceTime::from_ticks(std::numeric_limits<std::int64_t>::max() - 5);
    clip.speed = Speed::normal();

    const auto s_overflow = clip_to_source(clip, ClipTime::from_ticks(10));
    EXPECT_FALSE(s_overflow.has_value());
    EXPECT_EQ(s_overflow.error().code(), core::ErrorCode::Overflow);

    Clip clip_span_ovf;
    clip_span_ovf.duration = core::Duration::from_ticks(std::numeric_limits<std::int64_t>::max());
    clip_span_ovf.speed = Speed::create(2, 1).value();
    const auto span_ovf = source_span(clip_span_ovf);
    EXPECT_FALSE(span_ovf.has_value());
    EXPECT_EQ(span_ovf.error().code(), core::ErrorCode::Overflow);

    Clip clip_end_ovf;
    clip_end_ovf.start = TimelineTime::from_ticks(std::numeric_limits<std::int64_t>::max() - 5);
    clip_end_ovf.duration = core::Duration::from_ticks(10);
    const auto end_ovf = clip_end(clip_end_ovf);
    EXPECT_FALSE(end_ovf.has_value());
    EXPECT_EQ(end_ovf.error().code(), core::ErrorCode::Overflow);
}

// REQUIRED Vector V7 (source_span):
// duration 5 s with speed 2/1 gives 10 s;
// duration 5 s with speed 1/2 gives 2.5 s;
// duration 1 tick with speed 1/3 gives 1 tick;
// duration 3 ticks with speed 1/3 gives 1 tick.
TEST(ClipTest, VectorV7SourceSpan) {
    Clip clip;

    clip.duration = core::Duration::from_ticks(5 * T);
    clip.speed = Speed::create(2, 1).value();
    auto span = source_span(clip);
    ASSERT_TRUE(span.has_value());
    EXPECT_EQ(span->ticks(), 10 * T);

    clip.speed = Speed::create(1, 2).value();
    span = source_span(clip);
    ASSERT_TRUE(span.has_value());
    EXPECT_EQ(span->ticks(), 1'764'000'000);  // 2.5 s

    clip.duration = core::Duration::from_ticks(1);
    clip.speed = Speed::create(1, 3).value();
    span = source_span(clip);
    ASSERT_TRUE(span.has_value());
    EXPECT_EQ(span->ticks(), 1);

    clip.duration = core::Duration::from_ticks(3);
    clip.speed = Speed::create(1, 3).value();
    span = source_span(clip);
    ASSERT_TRUE(span.has_value());
    EXPECT_EQ(span->ticks(), 1);
}

// REQUIRED Vector V8 (slip and slide, done by plain field edits in the test):
// Start from V1.
// SLIP: set source_in to 3 s; timeline 12 s now gives source 5 s, and start and duration are
// unchanged. SLIDE: set start to 12 s keeping source_in 2 s; timeline 12 s gives clip 0 and source
// 2 s.
TEST(ClipTest, VectorV8SlipAndSlide) {
    Clip clip;
    clip.start = TimelineTime::from_ticks(10 * T);
    clip.duration = core::Duration::from_ticks(5 * T);
    clip.source_in = SourceTime::from_ticks(2 * T);
    clip.speed = Speed::normal();

    // SLIP: change source_in to 3 s
    clip.source_in = SourceTime::from_ticks(3 * T);
    EXPECT_EQ(clip.start.ticks(), 10 * T);
    EXPECT_EQ(clip.duration.ticks(), 5 * T);
    const auto s_slip = timeline_to_source(clip, TimelineTime::from_ticks(12 * T));
    ASSERT_TRUE(s_slip.has_value());
    EXPECT_EQ(s_slip->ticks(), 5 * T);

    // Reset back to V1 base
    clip.source_in = SourceTime::from_ticks(2 * T);

    // SLIDE: change start to 12 s
    clip.start = TimelineTime::from_ticks(12 * T);
    const auto c_slide = timeline_to_clip(clip, TimelineTime::from_ticks(12 * T));
    ASSERT_TRUE(c_slide.has_value());
    EXPECT_EQ(c_slide->ticks(), 0);

    const auto s_slide = timeline_to_source(clip, TimelineTime::from_ticks(12 * T));
    ASSERT_TRUE(s_slide.has_value());
    EXPECT_EQ(s_slide->ticks(), 2 * T);
}

// REQUIRED: speed 1/1 round trips for 1,000 pseudo-random (fixed seed, deterministic) clip times:
// source_to_clip(clip_to_source(c)) == c.
TEST(ClipTest, RoundTripSpeedOneToOne) {
    Clip clip;
    clip.start = TimelineTime::zero();
    clip.duration = core::Duration::from_ticks(100 * T);
    clip.source_in = SourceTime::from_ticks(5 * T);
    clip.speed = Speed::normal();

    std::mt19937_64 rng(424242ULL);
    std::uniform_int_distribution<std::int64_t> dist(0, 100 * T);

    for (int i = 0; i < 1000; ++i) {
        const ClipTime c = ClipTime::from_ticks(dist(rng));
        const auto s = clip_to_source(clip, c);
        ASSERT_TRUE(s.has_value()) << "clip_to_source failed at iteration " << i;

        const auto c_back = source_to_clip(clip, *s);
        ASSERT_TRUE(c_back.has_value()) << "source_to_clip failed at iteration " << i;
        EXPECT_EQ(*c_back, c) << "Mismatch at iteration " << i;
    }
}

}  // namespace
}  // namespace nxtcut::model
