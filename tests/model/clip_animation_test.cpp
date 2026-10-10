#include <nxtcut/core/color.hpp>
#include <nxtcut/core/error.hpp>
#include <nxtcut/core/result.hpp>
#include <nxtcut/core/time.hpp>
#include <nxtcut/core/uuid.hpp>
#include <nxtcut/keyframes/animatable.hpp>
#include <nxtcut/keyframes/easing.hpp>
#include <nxtcut/keyframes/interpolation.hpp>
#include <nxtcut/keyframes/keyframe.hpp>
#include <nxtcut/keyframes/keyframe_track.hpp>
#include <nxtcut/model/clip.hpp>
#include <nxtcut/model/clip_animation.hpp>
#include <nxtcut/model/effect.hpp>
#include <nxtcut/model/equality.hpp>
#include <nxtcut/model/ids.hpp>
#include <nxtcut/model/property.hpp>

#include <gtest/gtest.h>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include <nxtcut_test/assertions.hpp>

namespace nxtcut::model::test {
namespace {

using nxtcut::test::is_error;
using nxtcut::test::is_ok;

core::UuidGenerator& test_gen() {
    static core::UuidGenerator gen(901ULL);
    return gen;
}

Clip make_video_clip() {
    Clip clip;
    clip.id = generate_id<ClipId>(test_gen());
    clip.name = "TestVideo";
    clip.start = TimelineTime::zero();
    clip.duration = core::Duration::from_ticks(1000);
    clip.content = VideoContent{generate_id<MediaId>(test_gen())};
    return clip;
}

Clip make_audio_clip() {
    Clip clip;
    clip.id = generate_id<ClipId>(test_gen());
    clip.name = "TestAudio";
    clip.start = TimelineTime::zero();
    clip.duration = core::Duration::from_ticks(1000);
    clip.content = AudioContent{generate_id<MediaId>(test_gen())};
    return clip;
}

Clip make_text_clip() {
    Clip clip;
    clip.id = generate_id<ClipId>(test_gen());
    clip.name = "TestText";
    clip.start = TimelineTime::zero();
    clip.duration = core::Duration::from_ticks(1000);
    clip.content = TextContent{};
    return clip;
}

void expect_same_opacity_motion(const Clip& orig, const Clip& result, std::int64_t offset_ticks,
                                const std::vector<std::int64_t>& samples) {
    for (std::int64_t t : samples) {
        const auto res_v = result.transform.opacity.value_at(ClipTime::from_ticks(t));
        const auto orig_v = orig.transform.opacity.value_at(ClipTime::from_ticks(t + offset_ticks));
        EXPECT_EQ(std::bit_cast<std::uint64_t>(res_v), std::bit_cast<std::uint64_t>(orig_v))
            << "opacity mismatch at t=" << t << " with offset=" << offset_ticks;
    }
}

// M1: shift_keyframes(+/-)
TEST(ClipAnimationTest, M1_ShiftKeyframesPlusMinus) {
    Clip orig = make_video_clip();
    std::vector<keyframes::Keyframe<double>> keys{
        {core::TimePoint::from_ticks(0), 0.0, keyframes::Interpolation::linear()},
        {core::TimePoint::from_ticks(100), 1.0, keyframes::Interpolation::linear()},
    };
    auto track_res = keyframes::KeyframeTrack<double>::create(std::move(keys));
    ASSERT_TRUE(track_res.has_value());
    orig.transform.opacity.set_keyframes(std::move(track_res.value()));

    // Shift by -40
    Clip shifted_neg = orig;
    auto status_neg = shift_keyframes(shifted_neg, core::Duration::from_ticks(-40));
    EXPECT_TRUE(is_ok(status_neg));

    ASSERT_TRUE(shifted_neg.transform.opacity.is_animated());
    auto neg_keys = shifted_neg.transform.opacity.keyframes()->keys();
    ASSERT_EQ(neg_keys.size(), 2u);
    EXPECT_EQ(neg_keys[0].time.ticks(), -40);
    EXPECT_EQ(neg_keys[0].value, 0.0);
    EXPECT_EQ(neg_keys[1].time.ticks(), 60);
    EXPECT_EQ(neg_keys[1].value, 1.0);

    const std::vector<std::int64_t> samples{-80, -40, -20, 0, 10, 30, 50, 60, 80, 100, 150};
    // result.value_at(t) == orig.value_at(t + 40)
    expect_same_opacity_motion(orig, shifted_neg, 40, samples);
    // orig.value_at(t) == result.value_at(t - 40)
    expect_same_opacity_motion(shifted_neg, orig, -40, samples);

    // Shift by +40
    Clip shifted_pos = orig;
    auto status_pos = shift_keyframes(shifted_pos, core::Duration::from_ticks(40));
    EXPECT_TRUE(is_ok(status_pos));
    auto pos_keys = shifted_pos.transform.opacity.keyframes()->keys();
    ASSERT_EQ(pos_keys.size(), 2u);
    EXPECT_EQ(pos_keys[0].time.ticks(), 40);
    EXPECT_EQ(pos_keys[1].time.ticks(), 140);
    expect_same_opacity_motion(orig, shifted_pos, -40, samples);
}

// M2: non-animated clip: shift and scale return success, clip identical to before
TEST(ClipAnimationTest, M2_NonAnimatedClip) {
    Clip clip = make_video_clip();
    Clip before = clip;

    EXPECT_TRUE(is_ok(shift_keyframes(clip, core::Duration::from_ticks(50))));
    EXPECT_TRUE(identical(clip, before));

    EXPECT_TRUE(is_ok(scale_keyframes(clip, 2, 1)));
    EXPECT_TRUE(identical(clip, before));

    for_each_animatable_property(clip, [](const auto& prop) { EXPECT_FALSE(prop.is_animated()); });
}

// M3: offset 0 and ratio 1/1: track pointers unchanged
TEST(ClipAnimationTest, M3_OffsetZeroAndRatioOnePreserveTrackPointers) {
    Clip clip = make_video_clip();
    std::vector<keyframes::Keyframe<double>> keys{
        {core::TimePoint::from_ticks(0), 0.5, keyframes::Interpolation::linear()},
        {core::TimePoint::from_ticks(50), 1.0, keyframes::Interpolation::linear()},
    };
    auto track_res = keyframes::KeyframeTrack<double>::create(std::move(keys));
    ASSERT_TRUE(track_res.has_value());
    clip.transform.opacity.set_keyframes(std::move(track_res.value()));

    const auto* orig_ptr = clip.transform.opacity.keyframes();
    ASSERT_NE(orig_ptr, nullptr);

    EXPECT_TRUE(is_ok(shift_keyframes(clip, core::Duration::zero())));
    EXPECT_EQ(clip.transform.opacity.keyframes(), orig_ptr);

    EXPECT_TRUE(is_ok(scale_keyframes(clip, 1, 1)));
    EXPECT_EQ(clip.transform.opacity.keyframes(), orig_ptr);

    EXPECT_TRUE(is_ok(scale_keyframes(clip, 5, 5)));
    EXPECT_EQ(clip.transform.opacity.keyframes(), orig_ptr);
}

// M4: range checks: key at kMaxKeyframeTicks shifted by +1 -> InvalidArgument; -kMaxKeyframeTicks
// by -1 -> error
TEST(ClipAnimationTest, M4_RangeChecks) {
    Clip clip = make_video_clip();
    std::vector<keyframes::Keyframe<double>> keys{
        {core::TimePoint::from_ticks(keyframes::kMaxKeyframeTicks), 1.0,
         keyframes::Interpolation::linear()},
    };
    auto track_res = keyframes::KeyframeTrack<double>::create(std::move(keys));
    ASSERT_TRUE(track_res.has_value());
    clip.transform.opacity.set_keyframes(std::move(track_res.value()));

    auto st1 = shift_keyframes(clip, core::Duration::from_ticks(1));
    EXPECT_TRUE(is_error(st1, core::ErrorCode::InvalidArgument));

    std::vector<keyframes::Keyframe<double>> min_keys{
        {core::TimePoint::from_ticks(-keyframes::kMaxKeyframeTicks), 1.0,
         keyframes::Interpolation::linear()},
    };
    auto min_track_res = keyframes::KeyframeTrack<double>::create(std::move(min_keys));
    ASSERT_TRUE(min_track_res.has_value());
    clip.transform.opacity.set_keyframes(std::move(min_track_res.value()));

    auto st2 = shift_keyframes(clip, core::Duration::from_ticks(-1));
    EXPECT_TRUE(is_error(st2, core::ErrorCode::InvalidArgument));
}

// M5: scale: keys {0,100,200} x 3/2 -> {0,150,300}; x 1/2 -> {0,50,100}; keys {3} x 1/2 -> {2};
// keys {-3} x 1/2 -> {-2}
TEST(ClipAnimationTest, M5_ScaleKeyframes) {
    Clip clip = make_video_clip();
    std::vector<keyframes::Keyframe<double>> keys{
        {core::TimePoint::from_ticks(0), 0.1,
         keyframes::Interpolation::easing(keyframes::EasingKind::EaseInOutQuad)},
        {core::TimePoint::from_ticks(100), 0.5, keyframes::Interpolation::linear()},
        {core::TimePoint::from_ticks(200), 0.9, keyframes::Interpolation::hold()},
    };
    auto track_res = keyframes::KeyframeTrack<double>::create(keys);
    ASSERT_TRUE(track_res.has_value());
    clip.transform.opacity.set_keyframes(std::move(track_res.value()));

    // x 3/2 -> {0, 150, 300}
    Clip c1 = clip;
    EXPECT_TRUE(is_ok(scale_keyframes(c1, 3, 2)));
    auto k1 = c1.transform.opacity.keyframes()->keys();
    ASSERT_EQ(k1.size(), 3u);
    EXPECT_EQ(k1[0].time.ticks(), 0);
    EXPECT_EQ(k1[0].value, 0.1);
    EXPECT_EQ(k1[0].interpolation.kind(), keyframes::InterpolationKind::Easing);
    EXPECT_EQ(k1[1].time.ticks(), 150);
    EXPECT_EQ(k1[1].value, 0.5);
    EXPECT_EQ(k1[1].interpolation.kind(), keyframes::InterpolationKind::Linear);
    EXPECT_EQ(k1[2].time.ticks(), 300);
    EXPECT_EQ(k1[2].value, 0.9);

    // x 1/2 -> {0, 50, 100}
    Clip c2 = clip;
    EXPECT_TRUE(is_ok(scale_keyframes(c2, 1, 2)));
    auto k2 = c2.transform.opacity.keyframes()->keys();
    ASSERT_EQ(k2.size(), 3u);
    EXPECT_EQ(k2[0].time.ticks(), 0);
    EXPECT_EQ(k2[1].time.ticks(), 50);
    EXPECT_EQ(k2[2].time.ticks(), 100);

    // key {3} x 1/2 -> {2}
    Clip c3 = make_video_clip();
    auto tr3 = keyframes::KeyframeTrack<double>::create(
        {{core::TimePoint::from_ticks(3), 0.75, keyframes::Interpolation::linear()}});
    ASSERT_TRUE(tr3.has_value());
    c3.transform.opacity.set_keyframes(std::move(tr3.value()));
    EXPECT_TRUE(is_ok(scale_keyframes(c3, 1, 2)));
    EXPECT_EQ(c3.transform.opacity.keyframes()->keys()[0].time.ticks(), 2);

    // key {-3} x 1/2 -> {-2}
    Clip c4 = make_video_clip();
    auto tr4 = keyframes::KeyframeTrack<double>::create(
        {{core::TimePoint::from_ticks(-3), 0.75, keyframes::Interpolation::linear()}});
    ASSERT_TRUE(tr4.has_value());
    c4.transform.opacity.set_keyframes(std::move(tr4.value()));
    EXPECT_TRUE(is_ok(scale_keyframes(c4, 1, 2)));
    EXPECT_EQ(c4.transform.opacity.keyframes()->keys()[0].time.ticks(), -2);
}

// M6: collision: keys {10,11} x 1/4 -> InvalidArgument message containing "keyframe collision"
// numerator 0 or denominator 0 or negative -> InvalidArgument. keys {2^62} x 4/1 -> error.
TEST(ClipAnimationTest, M6_CollisionAndScaleErrors) {
    Clip clip = make_video_clip();
    std::vector<keyframes::Keyframe<double>> keys{
        {core::TimePoint::from_ticks(10), 0.1, keyframes::Interpolation::linear()},
        {core::TimePoint::from_ticks(11), 0.9, keyframes::Interpolation::linear()},
    };
    auto track_res = keyframes::KeyframeTrack<double>::create(std::move(keys));
    ASSERT_TRUE(track_res.has_value());
    clip.transform.opacity.set_keyframes(std::move(track_res.value()));

    // 10/4 = 2.5 -> 3, 11/4 = 2.75 -> 3 => collision at tick 3
    auto coll_st = scale_keyframes(clip, 1, 4);
    EXPECT_TRUE(is_error(coll_st, core::ErrorCode::InvalidArgument));
    EXPECT_NE(coll_st.error().message().find("keyframe collision"), std::string::npos);

    // numerator <= 0 or denominator <= 0
    EXPECT_TRUE(is_error(scale_keyframes(clip, 0, 4), core::ErrorCode::InvalidArgument));
    EXPECT_TRUE(is_error(scale_keyframes(clip, 4, 0), core::ErrorCode::InvalidArgument));
    EXPECT_TRUE(is_error(scale_keyframes(clip, -1, 4), core::ErrorCode::InvalidArgument));
    EXPECT_TRUE(is_error(scale_keyframes(clip, 4, -1), core::ErrorCode::InvalidArgument));

    // key at 2^62 x 4/1 -> overflow
    Clip huge_clip = make_video_clip();
    const std::int64_t huge_tick = std::int64_t{1} << 62;
    auto huge_track = keyframes::KeyframeTrack<double>::create(
        {{core::TimePoint::from_ticks(huge_tick), 0.5, keyframes::Interpolation::linear()}});
    ASSERT_TRUE(huge_track.has_value());
    huge_clip.transform.opacity.set_keyframes(std::move(huge_track.value()));

    auto huge_st = scale_keyframes(huge_clip, 4, 1);
    EXPECT_FALSE(huge_st.has_value());
}

// M7: visitor counts and order
TEST(ClipAnimationTest, M7_VisitorCountsAndOrder) {
    // 1. Video clip -> 12 visited
    Clip vclip = make_video_clip();
    int v_count = 0;
    for_each_animatable_property(vclip, [&](auto& prop) {
        static_cast<void>(prop);
        ++v_count;
    });
    EXPECT_EQ(v_count, 12);

    // 2. Audio clip -> 13 visited
    Clip aclip = make_audio_clip();
    int a_count = 0;
    for_each_animatable_property(aclip, [&](auto& prop) {
        static_cast<void>(prop);
        ++a_count;
    });
    EXPECT_EQ(a_count, 13);

    // 3. Text clip with 1 effect having params {double, bool, Color, string} -> 12 + 2 + 2 = 16
    Clip tclip = make_text_clip();
    EffectInstance eff;
    eff.id = generate_id<EffectId>(test_gen());
    eff.effect_type = "test.effect";
    // Set distinct values to verify visitor sequence
    tclip.transform.position_x.set_constant(1.0);
    tclip.transform.position_y.set_constant(2.0);
    tclip.transform.scale_x.set_constant(3.0);
    tclip.transform.scale_y.set_constant(4.0);
    tclip.transform.rotation_degrees.set_constant(5.0);
    tclip.transform.anchor_x.set_constant(6.0);
    tclip.transform.anchor_y.set_constant(7.0);
    tclip.transform.opacity.set_constant(8.0);
    tclip.transform.crop_left.set_constant(9.0);
    tclip.transform.crop_right.set_constant(10.0);
    tclip.transform.crop_top.set_constant(11.0);
    tclip.transform.crop_bottom.set_constant(12.0);

    auto& text_data = std::get<TextContent>(tclip.content);
    text_data.font_size_px.set_constant(13.0);
    text_data.color.set_constant(core::Color{14.0f, 0.0f, 0.0f, 1.0f});

    // Effect params in map: "param_a" = double (15.0), "param_b" = bool, "param_c" = Color (16.0f),
    // "param_d" = string
    eff.params["param_a"] = Property<double>{15.0};
    eff.params["param_b"] = Property<bool>{true};
    eff.params["param_c"] = Property<core::Color>{core::Color{16.0f, 0.0f, 0.0f, 1.0f}};
    eff.params["param_d"] = Property<std::string>{"ignored"};
    tclip.effects.push_back(std::move(eff));

    std::vector<int> visit_ids;
    for_each_animatable_property(tclip, [&](auto& prop) {
        using PropT = std::decay_t<decltype(prop)>;
        if constexpr (std::is_same_v<PropT, Property<double>>) {
            visit_ids.push_back(static_cast<int>(prop.constant_value()));
        } else if constexpr (std::is_same_v<PropT, Property<core::Color>>) {
            visit_ids.push_back(static_cast<int>(prop.constant_value().r));
        }
    });

    ASSERT_EQ(visit_ids.size(), 16u);
    for (int i = 0; i < 16; ++i) {
        EXPECT_EQ(visit_ids[static_cast<std::size_t>(i)], i + 1);
    }

    // Const overload test
    const Clip& const_tclip = tclip;
    int const_count = 0;
    for_each_animatable_property(const_tclip, [&](const auto& prop) {
        static_cast<void>(prop);
        ++const_count;
    });
    EXPECT_EQ(const_count, 16);
}

// M8: color tracks: a Property<core::Color> track on a text clip is shifted/scaled like doubles
TEST(ClipAnimationTest, M8_ColorTracksShiftAndScale) {
    Clip tclip = make_text_clip();
    const core::Color c1{1.0f, 0.0f, 0.0f, 1.0f};
    const core::Color c2{0.0f, 1.0f, 0.0f, 1.0f};

    std::vector<keyframes::Keyframe<core::Color>> keys{
        {core::TimePoint::from_ticks(0), c1, keyframes::Interpolation::linear()},
        {core::TimePoint::from_ticks(100), c2, keyframes::Interpolation::linear()},
    };
    auto track_res = keyframes::KeyframeTrack<core::Color>::create(std::move(keys));
    ASSERT_TRUE(track_res.has_value());

    auto& text_data = std::get<TextContent>(tclip.content);
    text_data.color.set_keyframes(std::move(track_res.value()));

    // Shift by -40
    EXPECT_TRUE(is_ok(shift_keyframes(tclip, core::Duration::from_ticks(-40))));
    auto shifted_keys = text_data.color.keyframes()->keys();
    ASSERT_EQ(shifted_keys.size(), 2u);
    EXPECT_EQ(shifted_keys[0].time.ticks(), -40);
    EXPECT_TRUE(keyframes::AnimatableTraits<core::Color>::identical(shifted_keys[0].value, c1));
    EXPECT_EQ(shifted_keys[1].time.ticks(), 60);
    EXPECT_TRUE(keyframes::AnimatableTraits<core::Color>::identical(shifted_keys[1].value, c2));

    // Scale x 3/2
    EXPECT_TRUE(is_ok(scale_keyframes(tclip, 3, 2)));
    auto scaled_keys = text_data.color.keyframes()->keys();
    ASSERT_EQ(scaled_keys.size(), 2u);
    EXPECT_EQ(scaled_keys[0].time.ticks(), -60);
    EXPECT_TRUE(keyframes::AnimatableTraits<core::Color>::identical(scaled_keys[0].value, c1));
    EXPECT_EQ(scaled_keys[1].time.ticks(), 90);
    EXPECT_TRUE(keyframes::AnimatableTraits<core::Color>::identical(scaled_keys[1].value, c2));
}

}  // namespace
}  // namespace nxtcut::model::test
