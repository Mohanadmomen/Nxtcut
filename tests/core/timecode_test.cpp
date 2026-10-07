#include <nxtcut/core/frame_rate.hpp>
#include <nxtcut/core/frame_time.hpp>
#include <nxtcut/core/timecode.hpp>

#include <gtest/gtest.h>

namespace nxtcut::core {
namespace {

TEST(TimecodeTest, RequiredTestVectors24Ndf) {
    const FrameRate rate = frame_rates::k24;

    // frame 0 -> "00:00:00:00"
    const auto tc0 = Timecode::from_frame(FrameIndex(0), rate, DropFrameMode::NonDrop);
    ASSERT_TRUE(tc0.has_value());
    EXPECT_EQ(tc0->to_string(), "00:00:00:00");
    EXPECT_EQ(tc0->to_frame(rate)->value(), 0);

    // frame 86400 -> "01:00:00:00"
    const auto tc86400 = Timecode::from_frame(FrameIndex(86400), rate, DropFrameMode::NonDrop);
    ASSERT_TRUE(tc86400.has_value());
    EXPECT_EQ(tc86400->to_string(), "01:00:00:00");
    EXPECT_EQ(tc86400->to_frame(rate)->value(), 86400);
}

TEST(TimecodeTest, RequiredTestVectors2997Df) {
    const FrameRate rate = frame_rates::k29_97;

    // frame 1799 -> "00:00:59;29"
    const auto tc1799 = Timecode::from_frame(FrameIndex(1799), rate, DropFrameMode::Drop);
    ASSERT_TRUE(tc1799.has_value());
    EXPECT_EQ(tc1799->to_string(), "00:00:59;29");
    EXPECT_EQ(tc1799->to_frame(rate)->value(), 1799);

    // frame 1800 -> "00:01:00;02"
    const auto tc1800 = Timecode::from_frame(FrameIndex(1800), rate, DropFrameMode::Drop);
    ASSERT_TRUE(tc1800.has_value());
    EXPECT_EQ(tc1800->to_string(), "00:01:00;02");
    EXPECT_EQ(tc1800->to_frame(rate)->value(), 1800);

    // frame 17982 -> "00:10:00;00"
    const auto tc17982 = Timecode::from_frame(FrameIndex(17982), rate, DropFrameMode::Drop);
    ASSERT_TRUE(tc17982.has_value());
    EXPECT_EQ(tc17982->to_string(), "00:10:00;00");
    EXPECT_EQ(tc17982->to_frame(rate)->value(), 17982);

    // frame 107892 -> "01:00:00;00"
    const auto tc107892 = Timecode::from_frame(FrameIndex(107892), rate, DropFrameMode::Drop);
    ASSERT_TRUE(tc107892.has_value());
    EXPECT_EQ(tc107892->to_string(), "01:00:00;00");
    EXPECT_EQ(tc107892->to_frame(rate)->value(), 107892);
}

TEST(TimecodeTest, RequiredTestVectors5994Df) {
    const FrameRate rate = frame_rates::k59_94;

    // frame 3600 -> "00:01:00;04"
    const auto tc3600 = Timecode::from_frame(FrameIndex(3600), rate, DropFrameMode::Drop);
    ASSERT_TRUE(tc3600.has_value());
    EXPECT_EQ(tc3600->to_string(), "00:01:00;04");
    EXPECT_EQ(tc3600->to_frame(rate)->value(), 3600);

    // frame 35964 -> "00:10:00;00"
    const auto tc35964 = Timecode::from_frame(FrameIndex(35964), rate, DropFrameMode::Drop);
    ASSERT_TRUE(tc35964.has_value());
    EXPECT_EQ(tc35964->to_string(), "00:10:00;00");
    EXPECT_EQ(tc35964->to_frame(rate)->value(), 35964);
}

TEST(TimecodeTest, RoundTripRange200000FramesAt2997Df) {
    const FrameRate rate = frame_rates::k29_97;
    for (std::int64_t f = 0; f <= 200'000; ++f) {
        const auto tc = Timecode::from_frame(FrameIndex(f), rate, DropFrameMode::Drop);
        ASSERT_TRUE(tc.has_value());
        const auto back = tc->to_frame(rate);
        ASSERT_TRUE(back.has_value());
        ASSERT_EQ(back->value(), f) << "Failed round-trip at frame " << f;
    }
}

TEST(TimecodeTest, RoundTripRange200000FramesAt5994Df) {
    const FrameRate rate = frame_rates::k59_94;
    for (std::int64_t f = 0; f <= 200'000; ++f) {
        const auto tc = Timecode::from_frame(FrameIndex(f), rate, DropFrameMode::Drop);
        ASSERT_TRUE(tc.has_value());
        const auto back = tc->to_frame(rate);
        ASSERT_TRUE(back.has_value());
        ASSERT_EQ(back->value(), f) << "Failed round-trip at frame " << f;
    }
}

TEST(TimecodeTest, RoundTripRange200000FramesAt24Ndf) {
    const FrameRate rate = frame_rates::k24;
    for (std::int64_t f = 0; f <= 200'000; ++f) {
        const auto tc = Timecode::from_frame(FrameIndex(f), rate, DropFrameMode::NonDrop);
        ASSERT_TRUE(tc.has_value());
        const auto back = tc->to_frame(rate);
        ASSERT_TRUE(back.has_value());
        ASSERT_EQ(back->value(), f) << "Failed round-trip at frame " << f;
    }
}

TEST(TimecodeTest, RoundTripRange200000FramesAt25Ndf) {
    const FrameRate rate = frame_rates::k25;
    for (std::int64_t f = 0; f <= 200'000; ++f) {
        const auto tc = Timecode::from_frame(FrameIndex(f), rate, DropFrameMode::NonDrop);
        ASSERT_TRUE(tc.has_value());
        const auto back = tc->to_frame(rate);
        ASSERT_TRUE(back.has_value());
        ASSERT_EQ(back->value(), f) << "Failed round-trip at frame " << f;
    }
}

TEST(TimecodeTest, ParseRejectsDroppedFrameLabels) {
    const FrameRate rate2997 = frame_rates::k29_97;
    // Frame numbers 00 and 01 do not exist at minute 1 in 29.97 DF
    EXPECT_FALSE(Timecode::parse("00:01:00;00", rate2997).has_value());
    EXPECT_FALSE(Timecode::parse("00:01:00;01", rate2997).has_value());
    // But frame 02 exists
    EXPECT_TRUE(Timecode::parse("00:01:00;02", rate2997).has_value());

    // At minute 10, no frames dropped
    EXPECT_TRUE(Timecode::parse("00:10:00;00", rate2997).has_value());

    const FrameRate rate5994 = frame_rates::k59_94;
    // Frames 00, 01, 02, 03 do not exist at minute 1 in 59.94 DF
    EXPECT_FALSE(Timecode::parse("00:01:00;00", rate5994).has_value());
    EXPECT_FALSE(Timecode::parse("00:01:00;01", rate5994).has_value());
    EXPECT_FALSE(Timecode::parse("00:01:00;02", rate5994).has_value());
    EXPECT_FALSE(Timecode::parse("00:01:00;03", rate5994).has_value());
    EXPECT_TRUE(Timecode::parse("00:01:00;04", rate5994).has_value());
}

TEST(TimecodeTest, ParseValidFormats) {
    const FrameRate rate = frame_rates::k24;
    const auto tc = Timecode::parse("01:23:45:12", rate);
    ASSERT_TRUE(tc.has_value());
    EXPECT_EQ(tc->hours(), 1);
    EXPECT_EQ(tc->minutes(), 23);
    EXPECT_EQ(tc->seconds(), 45);
    EXPECT_EQ(tc->frames(), 12);
    EXPECT_FALSE(tc->is_drop_frame());
}

TEST(TimecodeTest, ParseRejectionInvalidInputs) {
    const FrameRate rate = frame_rates::k24;
    // Wrong length
    EXPECT_FALSE(Timecode::parse("00:00:00", rate).has_value());
    EXPECT_FALSE(Timecode::parse("00:00:00:000", rate).has_value());
    // Invalid delimiters
    EXPECT_FALSE(Timecode::parse("00-00-00-00", rate).has_value());
    // Non-numeric digits
    EXPECT_FALSE(Timecode::parse("00:00:00:ab", rate).has_value());
    // Out of range minutes/seconds
    EXPECT_FALSE(Timecode::parse("00:60:00:00", rate).has_value());
    EXPECT_FALSE(Timecode::parse("00:00:60:00", rate).has_value());
    // Out of range frames (>= 24 for 24fps)
    EXPECT_FALSE(Timecode::parse("00:00:00:24", rate).has_value());
    // Drop-frame on 24 fps
    EXPECT_FALSE(Timecode::parse("00:00:00;00", rate).has_value());
}

TEST(TimecodeTest, NegativeFrameRejection) {
    const auto res = Timecode::from_frame(FrameIndex(-1), frame_rates::k24, DropFrameMode::NonDrop);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), ErrorCode::OutOfRange);
}

TEST(TimecodeTest, UnsupportedDropFrameRate) {
    const auto res = Timecode::from_frame(FrameIndex(100), frame_rates::k24, DropFrameMode::Drop);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), ErrorCode::Unsupported);
}

}  // namespace
}  // namespace nxtcut::core
