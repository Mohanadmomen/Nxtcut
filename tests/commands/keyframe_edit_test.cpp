#include <nxtcut/commands/clip_commands.hpp>
#include <nxtcut/commands/editor.hpp>
#include <nxtcut/commands/timeline_commands.hpp>
#include <nxtcut/core/color.hpp>
#include <nxtcut/core/frame_rate.hpp>
#include <nxtcut/core/frame_time.hpp>
#include <nxtcut/core/mul_div.hpp>
#include <nxtcut/core/time.hpp>
#include <nxtcut/core/uuid.hpp>
#include <nxtcut/keyframes/easing.hpp>
#include <nxtcut/keyframes/interpolation.hpp>
#include <nxtcut/keyframes/keyframe.hpp>
#include <nxtcut/keyframes/keyframe_track.hpp>
#include <nxtcut/model/clip.hpp>
#include <nxtcut/model/clip_animation.hpp>
#include <nxtcut/model/equality.hpp>
#include <nxtcut/model/ids.hpp>
#include <nxtcut/model/project.hpp>
#include <nxtcut/model/property.hpp>
#include <nxtcut/model/speed.hpp>
#include <nxtcut/model/time_coords.hpp>

#include <gtest/gtest.h>

#include <bit>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "test_helper.hpp"
#include <nxtcut_test/assertions.hpp>
#include <nxtcut_test/model_fixtures.hpp>

namespace nxtcut::commands {
namespace {

using nxtcut::test::is_error;
using nxtcut::test::is_ok;

const model::Clip* find_clip_in(const model::Project& project, model::SequenceId seq,
                                model::ClipId id) {
    for (const auto& trk : project.sequences.at(seq).tracks) {
        for (const auto& cl : trk.clips) {
            if (cl.id == id) {
                return &cl;
            }
        }
    }
    return nullptr;
}

void animate_clip(model::Clip& clip, std::int64_t F) {
    auto bezier_res = keyframes::Interpolation::bezier(0.25, 0.1, 0.25, 1.0);
    EXPECT_TRUE(bezier_res.has_value());
    const auto bezier = bezier_res.value();

    std::vector<keyframes::Keyframe<double>> op_keys{
        {core::TimePoint::from_ticks(0), 0.0,
         keyframes::Interpolation::easing(keyframes::EasingKind::EaseInOutQuad)},
        {core::TimePoint::from_ticks(4 * F), 1.0, bezier},
        {core::TimePoint::from_ticks(8 * F), 0.25, keyframes::Interpolation::linear()},
    };
    auto op_track = keyframes::KeyframeTrack<double>::create(std::move(op_keys));
    EXPECT_TRUE(op_track.has_value());
    clip.transform.opacity.set_keyframes(std::move(op_track.value()));

    std::vector<keyframes::Keyframe<double>> pos_keys{
        {core::TimePoint::from_ticks(2 * F), -100.0, keyframes::Interpolation::linear()},
        {core::TimePoint::from_ticks(6 * F), 50.0, keyframes::Interpolation::linear()},
    };
    auto pos_track = keyframes::KeyframeTrack<double>::create(std::move(pos_keys));
    EXPECT_TRUE(pos_track.has_value());
    clip.transform.position_x.set_keyframes(std::move(pos_track.value()));
}

void animate_audio_clip(model::Clip& clip, std::int64_t F) {
    auto* audio = std::get_if<model::AudioContent>(&clip.content);
    ASSERT_NE(audio, nullptr);
    std::vector<keyframes::Keyframe<double>> vol_keys{
        {core::TimePoint::from_ticks(0), 1.0, keyframes::Interpolation::linear()},
        {core::TimePoint::from_ticks(8 * F), 0.0, keyframes::Interpolation::linear()},
    };
    auto vol_track = keyframes::KeyframeTrack<double>::create(std::move(vol_keys));
    ASSERT_TRUE(vol_track.has_value());
    audio->volume.set_keyframes(std::move(vol_track.value()));
}

std::vector<std::int64_t> make_samples(std::int64_t F) {
    return {-2 * F, -F,    -F / 2, 0,     F / 2, F,     2 * F,  3 * F,
            4 * F,  5 * F, 6 * F,  7 * F, 8 * F, 9 * F, 10 * F, 12 * F};
}

void expect_same_motion(const model::Clip& orig, const model::Clip& result,
                        std::int64_t offset_ticks, const std::vector<std::int64_t>& samples) {
    for (std::int64_t t : samples) {
        const auto res_op = result.transform.opacity.value_at(model::ClipTime::from_ticks(t));
        const auto orig_op =
            orig.transform.opacity.value_at(model::ClipTime::from_ticks(t + offset_ticks));
        EXPECT_EQ(std::bit_cast<std::uint64_t>(res_op), std::bit_cast<std::uint64_t>(orig_op))
            << "opacity mismatch at t=" << t << " with offset=" << offset_ticks;

        const auto res_pos = result.transform.position_x.value_at(model::ClipTime::from_ticks(t));
        const auto orig_pos =
            orig.transform.position_x.value_at(model::ClipTime::from_ticks(t + offset_ticks));
        EXPECT_EQ(std::bit_cast<std::uint64_t>(res_pos), std::bit_cast<std::uint64_t>(orig_pos))
            << "position_x mismatch at t=" << t << " with offset=" << offset_ticks;
    }
}

void expect_scaled_motion(const model::Clip& orig, const model::Clip& result, std::int64_t num,
                          std::int64_t den, const std::vector<std::int64_t>& orig_samples) {
    for (std::int64_t t : orig_samples) {
        const auto scaled_t_res = core::mul_div(t, num, den, core::RoundingMode::Nearest);
        ASSERT_TRUE(scaled_t_res.has_value());
        const std::int64_t scaled_t = *scaled_t_res;

        const auto res_op =
            result.transform.opacity.value_at(model::ClipTime::from_ticks(scaled_t));
        const auto orig_op = orig.transform.opacity.value_at(model::ClipTime::from_ticks(t));
        EXPECT_EQ(std::bit_cast<std::uint64_t>(res_op), std::bit_cast<std::uint64_t>(orig_op))
            << "scaled opacity mismatch at orig t=" << t << ", scaled t=" << scaled_t;

        const auto res_pos =
            result.transform.position_x.value_at(model::ClipTime::from_ticks(scaled_t));
        const auto orig_pos = orig.transform.position_x.value_at(model::ClipTime::from_ticks(t));
        EXPECT_EQ(std::bit_cast<std::uint64_t>(res_pos), std::bit_cast<std::uint64_t>(orig_pos))
            << "scaled position_x mismatch at orig t=" << t << ", scaled t=" << scaled_t;
    }
}

// C1: SplitClip at start + 3F
TEST(KeyframeEditTest, C1_SplitClipKeyframeShift) {
    core::UuidGenerator gen(9001ULL);
    auto project = model::test::build_valid_project(gen);
    const auto seq_id = project.main_sequence;
    auto& seq = project.sequences.at(seq_id);
    const std::int64_t F = core::frame_duration(seq.frame_rate)->ticks();

    // Isolate clip 0 on video track: duration 16F, animated
    auto& clip0 = seq.tracks[0].clips[0];
    clip0.start = model::TimelineTime::zero();
    clip0.duration = core::Duration::from_ticks(16 * F);
    clip0.link_id = std::nullopt;
    seq.tracks[1].clips[0].link_id = std::nullopt;
    animate_clip(clip0, F);

    const auto orig_clip = clip0;
    const auto* orig_op_ptr = clip0.transform.opacity.keyframes();
    const auto* orig_pos_ptr = clip0.transform.position_x.keyframes();
    const auto clip_id = clip0.id;

    auto ed_res = Editor::create(std::move(project), gen);
    ASSERT_TRUE(is_ok(ed_res));
    Editor& editor = *ed_res.value();

    SplitClip cmd{seq_id, clip_id, model::TimelineTime::from_ticks(3 * F), true};
    auto rt = test::round_trip(editor, cmd);
    ASSERT_TRUE(is_ok(rt));

    const auto& v_clips = editor.snapshot()->sequences.at(seq_id).tracks[0].clips;
    ASSERT_GE(v_clips.size(), 2u);
    const auto& left = v_clips[0];
    const auto& right = v_clips[1];

    EXPECT_EQ(left.id, clip_id);
    EXPECT_EQ(left.duration.ticks(), 3 * F);
    EXPECT_EQ(left.transform.opacity.keyframes(), orig_op_ptr);
    EXPECT_EQ(left.transform.position_x.keyframes(), orig_pos_ptr);

    EXPECT_NE(right.id, clip_id);
    EXPECT_EQ(right.start.ticks(), 3 * F);
    EXPECT_EQ(right.duration.ticks(), 13 * F);
    ASSERT_TRUE(right.transform.opacity.is_animated());

    auto r_op_keys = right.transform.opacity.keyframes()->keys();
    ASSERT_EQ(r_op_keys.size(), 3u);
    EXPECT_EQ(r_op_keys[0].time.ticks(), 0 - 3 * F);
    EXPECT_EQ(r_op_keys[1].time.ticks(), 4 * F - 3 * F);
    EXPECT_EQ(r_op_keys[2].time.ticks(), 8 * F - 3 * F);

    const auto samples = make_samples(F);
    expect_same_motion(orig_clip, left, 0, samples);
    expect_same_motion(orig_clip, right, 3 * F, samples);

    // Non-animated split verification
    // Make sure non-animated clip is tested
    auto& non_anim_clip = v_clips[2];  // Image clip on track 0
    EXPECT_FALSE(non_anim_clip.transform.opacity.is_animated());
    const auto non_anim_id = non_anim_clip.id;
    const auto non_anim_at = model::TimelineTime::from_ticks(non_anim_clip.start.ticks() + 2 * F);

    SplitClip na_cmd{seq_id, non_anim_id, non_anim_at, true};
    auto na_rt = test::round_trip(editor, na_cmd);
    ASSERT_TRUE(is_ok(na_rt));
    const auto& updated_clips = editor.snapshot()->sequences.at(seq_id).tracks[0].clips;
    for (const auto& cl : updated_clips) {
        if (cl.content.index() == 2) {  // ImageContent
            EXPECT_FALSE(cl.transform.opacity.is_animated());
            EXPECT_FALSE(cl.transform.position_x.is_animated());
        }
    }
}

// C2: Linked pair (video + audio, audio volume keys {0: 1.0, 8F: 0.0})
TEST(KeyframeEditTest, C2_LinkedPairSplitClip) {
    core::UuidGenerator gen(9002ULL);
    auto project = model::test::build_valid_project(gen);
    const auto seq_id = project.main_sequence;
    auto& seq = project.sequences.at(seq_id);
    const std::int64_t F = core::frame_duration(seq.frame_rate)->ticks();

    auto& v_clip = seq.tracks[0].clips[0];
    auto& a_clip = seq.tracks[1].clips[0];
    v_clip.start = model::TimelineTime::zero();
    v_clip.duration = core::Duration::from_ticks(16 * F);
    a_clip.start = model::TimelineTime::zero();
    a_clip.duration = core::Duration::from_ticks(16 * F);

    animate_clip(v_clip, F);
    animate_audio_clip(a_clip, F);

    const auto orig_v = v_clip;
    const auto orig_a = a_clip;
    const auto v_id = v_clip.id;

    auto ed_res = Editor::create(std::move(project), gen);
    ASSERT_TRUE(is_ok(ed_res));
    Editor& editor = *ed_res.value();

    SplitClip cmd{seq_id, v_id, model::TimelineTime::from_ticks(3 * F), false};
    auto rt = test::round_trip(editor, cmd);
    ASSERT_TRUE(is_ok(rt));

    const auto& v_clips = editor.snapshot()->sequences.at(seq_id).tracks[0].clips;
    const auto& a_clips = editor.snapshot()->sequences.at(seq_id).tracks[1].clips;

    // Both right halves shifted by -3F
    const auto& r_video = v_clips[1];
    const auto& r_audio = a_clips[1];
    EXPECT_EQ(r_video.start.ticks(), 3 * F);
    EXPECT_EQ(r_audio.start.ticks(), 3 * F);

    const auto samples = make_samples(F);
    expect_same_motion(orig_v, r_video, 3 * F, samples);

    const auto* r_audio_content = std::get_if<model::AudioContent>(&r_audio.content);
    ASSERT_NE(r_audio_content, nullptr);
    ASSERT_TRUE(r_audio_content->volume.is_animated());
    auto r_vol_keys = r_audio_content->volume.keyframes()->keys();
    ASSERT_EQ(r_vol_keys.size(), 2u);
    EXPECT_EQ(r_vol_keys[0].time.ticks(), 0 - 3 * F);
    EXPECT_EQ(r_vol_keys[0].value, 1.0);
    EXPECT_EQ(r_vol_keys[1].time.ticks(), 8 * F - 3 * F);
    EXPECT_EQ(r_vol_keys[1].value, 0.0);

    // Left halves unchanged
    const auto& l_video = v_clips[0];
    const auto& l_audio = a_clips[0];
    EXPECT_EQ(l_video.transform.opacity.keyframes(), orig_v.transform.opacity.keyframes());
    const auto* l_audio_content = std::get_if<model::AudioContent>(&l_audio.content);
    ASSERT_NE(l_audio_content, nullptr);
    EXPECT_EQ(l_audio_content->volume.keyframes(),
              std::get<model::AudioContent>(orig_a.content).volume.keyframes());
}

// C3: TrimClip head non-ripple start+2F, extend head by -F, tail trims, ripple head trim
TEST(KeyframeEditTest, C3_TrimClipVariations) {
    core::UuidGenerator gen(9003ULL);
    auto project = model::test::build_valid_project(gen);
    const auto seq_id = project.main_sequence;
    auto& seq = project.sequences.at(seq_id);
    const std::int64_t F = core::frame_duration(seq.frame_rate)->ticks();

    // Setup clip at start=2F, duration=16F, source_in=2F (allows extending head by -F)
    auto& clip0 = seq.tracks[0].clips[0];
    clip0.start = model::TimelineTime::from_ticks(2 * F);
    clip0.duration = core::Duration::from_ticks(16 * F);
    clip0.source_in = model::SourceTime::from_ticks(2 * F);
    clip0.link_id = std::nullopt;
    seq.tracks[1].clips[0].link_id = std::nullopt;
    animate_clip(clip0, F);

    const auto orig_clip = clip0;
    const auto* orig_ptr = clip0.transform.opacity.keyframes();
    const auto clip_id = clip0.id;
    const auto samples = make_samples(F);

    auto ed_res = Editor::create(std::move(project), gen);
    ASSERT_TRUE(is_ok(ed_res));
    Editor& editor = *ed_res.value();

    // 1. Head non-ripple trim to start+2F (new_edge = 4F)
    {
        TrimClip cmd{seq_id,
                     clip_id,
                     TrimEdge::Head,
                     model::TimelineTime::from_ticks(4 * F),
                     false,
                     RippleScope::AllUnlockedTracks,
                     true};
        auto rt = test::round_trip(editor, cmd);
        ASSERT_TRUE(is_ok(rt));
        const auto& cl = editor.snapshot()->sequences.at(seq_id).tracks[0].clips[0];
        EXPECT_EQ(cl.start.ticks(), 4 * F);
        expect_same_motion(orig_clip, cl, 2 * F, samples);
    }

    // 2. Extend head by -F (from 4F to 3F, or from 2F to F)
    // Revert/reset to start=2F for isolated test
    {
        auto undo_st = editor.undo();
        EXPECT_TRUE(is_ok(undo_st));
        TrimClip cmd{seq_id,
                     clip_id,
                     TrimEdge::Head,
                     model::TimelineTime::from_ticks(F),
                     false,
                     RippleScope::AllUnlockedTracks,
                     true};
        auto rt = test::round_trip(editor, cmd);
        ASSERT_TRUE(is_ok(rt));
        const auto& cl = editor.snapshot()->sequences.at(seq_id).tracks[0].clips[0];
        EXPECT_EQ(cl.start.ticks(), F);
        expect_same_motion(orig_clip, cl, -F, samples);
        EXPECT_TRUE(is_ok(editor.undo()));
    }

    // 3. Tail trim (shorter: new_edge = 14F; longer: new_edge = 17F): pointers unchanged
    {
        TrimClip cmd_shorter{seq_id,
                             clip_id,
                             TrimEdge::Tail,
                             model::TimelineTime::from_ticks(14 * F),
                             false,
                             RippleScope::AllUnlockedTracks,
                             true};
        auto rt = test::round_trip(editor, cmd_shorter);
        ASSERT_TRUE(is_ok(rt));
        const auto& cl_short = editor.snapshot()->sequences.at(seq_id).tracks[0].clips[0];
        EXPECT_EQ(cl_short.transform.opacity.keyframes(), orig_ptr);
        EXPECT_TRUE(is_ok(editor.undo()));

        TrimClip cmd_longer{seq_id,
                            clip_id,
                            TrimEdge::Tail,
                            model::TimelineTime::from_ticks(17 * F),
                            false,
                            RippleScope::AllUnlockedTracks,
                            true};
        auto rt_long = test::round_trip(editor, cmd_longer);
        ASSERT_TRUE(is_ok(rt_long));
        const auto& cl_long = editor.snapshot()->sequences.at(seq_id).tracks[0].clips[0];
        EXPECT_EQ(cl_long.transform.opacity.keyframes(), orig_ptr);
        EXPECT_TRUE(is_ok(editor.undo()));
    }

    // 4. Ripple head trim start+2F (new_edge = 4F)
    {
        TrimClip cmd{seq_id,
                     clip_id,
                     TrimEdge::Head,
                     model::TimelineTime::from_ticks(4 * F),
                     true,
                     RippleScope::AllUnlockedTracks,
                     true};
        auto rt = test::round_trip(editor, cmd);
        ASSERT_TRUE(is_ok(rt));
        const auto& cl = editor.snapshot()->sequences.at(seq_id).tracks[0].clips[0];
        EXPECT_EQ(cl.start.ticks(), 2 * F);  // Ripple head keeps start
        expect_same_motion(orig_clip, cl, 2 * F, samples);
    }
}

// C4: RollEdit and SlideClip
TEST(KeyframeEditTest, C4_RollEditAndSlideClip) {
    core::UuidGenerator gen(9004ULL);
    auto project = model::test::build_valid_project(gen);
    const auto seq_id = project.main_sequence;
    auto& seq = project.sequences.at(seq_id);
    const std::int64_t F = core::frame_duration(seq.frame_rate)->ticks();

    // Track 0: c1 (0..16F) and c2 (16F..32F), both animated video clips
    auto c1 = seq.tracks[0].clips[0];  // copy: the track's clip list is cleared below
    c1.start = model::TimelineTime::zero();
    c1.duration = core::Duration::from_ticks(16 * F);
    c1.link_id = std::nullopt;
    seq.tracks[1].clips[0].link_id = std::nullopt;
    animate_clip(c1, F);

    auto c2 = c1;
    c2.id = model::generate_id<model::ClipId>(gen);
    c2.start = model::TimelineTime::from_ticks(16 * F);
    c2.duration = core::Duration::from_ticks(16 * F);
    c2.source_in = model::SourceTime::from_ticks(4 * F);
    animate_clip(c2, F);

    seq.tracks[0].clips.clear();
    seq.tracks[0].clips.push_back(c1);
    seq.tracks[0].clips.push_back(c2);

    const auto orig_c1 = c1;
    const auto orig_c2 = c2;
    const auto* orig_c1_ptr = c1.transform.opacity.keyframes();
    const auto samples = make_samples(F);

    auto ed_res = Editor::create(std::move(project), gen);
    ASSERT_TRUE(is_ok(ed_res));
    Editor& editor = *ed_res.value();

    // RollEdit moving edit point by +2F to 18F
    RollEdit roll_cmd{seq_id, orig_c1.id, orig_c2.id, model::TimelineTime::from_ticks(18 * F),
                      true};
    auto rt_roll = test::round_trip(editor, roll_cmd);
    ASSERT_TRUE(is_ok(rt_roll));

    const auto& after_roll = editor.snapshot()->sequences.at(seq_id).tracks[0].clips;
    EXPECT_EQ(after_roll[0].transform.opacity.keyframes(), orig_c1_ptr);
    expect_same_motion(orig_c2, after_roll[1], 2 * F, samples);
}

// C4b: SlideClip
TEST(KeyframeEditTest, C4b_SlideClip) {
    core::UuidGenerator gen(9014ULL);
    auto project = model::test::build_valid_project(gen);
    const auto seq_id = project.main_sequence;
    auto& seq = project.sequences.at(seq_id);
    const std::int64_t F = core::frame_duration(seq.frame_rate)->ticks();
    const auto samples = make_samples(F);

    auto& c1 = seq.tracks[0].clips[0];
    c1.start = model::TimelineTime::zero();
    c1.duration = core::Duration::from_ticks(16 * F);
    c1.link_id = std::nullopt;
    seq.tracks[1].clips[0].link_id = std::nullopt;
    animate_clip(c1, F);

    // SlideClip setup: 3 adjacent clips: left (0..8F), slid (8F..24F), right (24F..32F)
    auto left_clip = c1;
    left_clip.duration = core::Duration::from_ticks(8 * F);

    auto slid_clip = c1;
    slid_clip.id = model::generate_id<model::ClipId>(gen);
    slid_clip.start = model::TimelineTime::from_ticks(8 * F);
    slid_clip.duration = core::Duration::from_ticks(16 * F);

    auto right_clip = c1;
    right_clip.id = model::generate_id<model::ClipId>(gen);
    right_clip.start = model::TimelineTime::from_ticks(24 * F);
    right_clip.duration = core::Duration::from_ticks(8 * F);
    right_clip.source_in = model::SourceTime::from_ticks(4 * F);

    seq.tracks[0].clips = {left_clip, slid_clip, right_clip};

    const auto orig_slid = slid_clip;
    const auto orig_right = right_clip;
    const auto* orig_slid_ptr = slid_clip.transform.opacity.keyframes();
    const auto* orig_left_ptr = left_clip.transform.opacity.keyframes();

    auto ed_res = Editor::create(std::move(project), gen);
    ASSERT_TRUE(is_ok(ed_res));
    Editor& editor = *ed_res.value();

    SlideClip slide_cmd{seq_id, slid_clip.id, model::TimelineTime::from_ticks(10 * F), true};
    auto rt_slide = test::round_trip(editor, slide_cmd);
    ASSERT_TRUE(is_ok(rt_slide));

    const auto& after_slide = editor.snapshot()->sequences.at(seq_id).tracks[0].clips;
    EXPECT_EQ(after_slide[0].transform.opacity.keyframes(), orig_left_ptr);
    EXPECT_EQ(after_slide[1].transform.opacity.keyframes(), orig_slid_ptr);
    expect_same_motion(orig_right, after_slide[2], 2 * F, samples);
}

// C5: Untouched: SlipClip by +2F, MoveClips by +10F, ShiftTrackClips +5F
TEST(KeyframeEditTest, C5_UntouchedEditsPreservePointers) {
    core::UuidGenerator gen(9005ULL);
    auto project = model::test::build_valid_project(gen);
    const auto seq_id = project.main_sequence;
    auto& seq = project.sequences.at(seq_id);
    const std::int64_t F = core::frame_duration(seq.frame_rate)->ticks();

    auto& c0 = seq.tracks[0].clips[0];
    c0.start = model::TimelineTime::zero();
    c0.duration = core::Duration::from_ticks(16 * F);
    c0.source_in = model::SourceTime::from_ticks(2 * F);
    c0.link_id = std::nullopt;
    seq.tracks[1].clips[0].link_id = std::nullopt;
    animate_clip(c0, F);

    const auto* orig_op_ptr = c0.transform.opacity.keyframes();
    const auto* orig_pos_ptr = c0.transform.position_x.keyframes();
    const auto cid = c0.id;
    const auto tid = seq.tracks[0].id;

    auto ed_res = Editor::create(std::move(project), gen);
    ASSERT_TRUE(is_ok(ed_res));
    Editor& editor = *ed_res.value();

    // SlipClip +2F
    SlipClip slip_cmd{seq_id, cid, core::Duration::from_ticks(2 * F), true};
    auto rt_slip = test::round_trip(editor, slip_cmd);
    ASSERT_TRUE(is_ok(rt_slip));
    {
        const auto& cl = editor.snapshot()->sequences.at(seq_id).tracks[0].clips[0];
        EXPECT_EQ(cl.transform.opacity.keyframes(), orig_op_ptr);
        EXPECT_EQ(cl.transform.position_x.keyframes(), orig_pos_ptr);
        EXPECT_TRUE(is_ok(editor.undo()));
    }

    // MoveClips +10F (move clip to start = 20F)
    MoveClips move_cmd{seq_id, {ClipMove{cid, model::TimelineTime::from_ticks(20 * F), tid}}, true};
    auto rt_move = test::round_trip(editor, move_cmd);
    ASSERT_TRUE(is_ok(rt_move));
    {
        const auto snap = editor.snapshot();
        const auto* cl = find_clip_in(*snap, seq_id, cid);
        ASSERT_NE(cl, nullptr);
        EXPECT_EQ(cl->transform.opacity.keyframes(), orig_op_ptr);
        EXPECT_EQ(cl->transform.position_x.keyframes(), orig_pos_ptr);
        EXPECT_TRUE(is_ok(editor.undo()));
    }

    // ShiftTrackClips +5F
    ShiftTrackClips shift_cmd{seq_id, tid, model::TimelineTime::zero(),
                              core::Duration::from_ticks(5 * F), true};
    auto rt_shift = test::round_trip(editor, shift_cmd);
    ASSERT_TRUE(is_ok(rt_shift));
    {
        const auto snap = editor.snapshot();
        const auto* cl = find_clip_in(*snap, seq_id, cid);
        ASSERT_NE(cl, nullptr);
        EXPECT_EQ(cl->transform.opacity.keyframes(), orig_op_ptr);
        EXPECT_EQ(cl->transform.position_x.keyframes(), orig_pos_ptr);
        EXPECT_TRUE(is_ok(editor.undo()));
    }
}

// C6: OverwriteClips and InsertClips
TEST(KeyframeEditTest, C6_OverwriteAndInsertClips) {
    core::UuidGenerator gen(9006ULL);
    auto project = model::test::build_valid_project(gen);
    const auto seq_id = project.main_sequence;
    auto& seq = project.sequences.at(seq_id);
    const std::int64_t F = core::frame_duration(seq.frame_rate)->ticks();

    auto& c0 = seq.tracks[0].clips[0];
    c0.start = model::TimelineTime::zero();
    c0.duration = core::Duration::from_ticks(16 * F);
    c0.link_id = std::nullopt;
    seq.tracks[1].clips[0].link_id = std::nullopt;
    animate_clip(c0, F);

    const auto orig_clip = c0;
    const auto* orig_ptr = c0.transform.opacity.keyframes();
    const auto samples = make_samples(F);
    // Captured before the project is moved into the editor (seq is invalid afterwards)
    const auto track0_id = seq.tracks[0].id;
    const model::Clip filler_clip = seq.tracks[0].clips[1];  // image clip

    auto ed_res = Editor::create(std::move(project), gen);
    ASSERT_TRUE(is_ok(ed_res));
    Editor& editor = *ed_res.value();

    // 1. Overwrite covering first 2F
    PlacedClip pc1;
    pc1.track = track0_id;
    pc1.clip = filler_clip;
    pc1.clip.duration = core::Duration::from_ticks(2 * F);

    OverwriteClips ov1{seq_id, model::TimelineTime::zero(), {pc1}};
    auto rt_ov1 = test::round_trip(editor, ov1);
    ASSERT_TRUE(is_ok(rt_ov1));
    {
        const auto& cl = editor.snapshot()->sequences.at(seq_id).tracks[0].clips[1];
        EXPECT_EQ(cl.start.ticks(), 2 * F);
        expect_same_motion(orig_clip, cl, 2 * F, samples);
        EXPECT_TRUE(is_ok(editor.undo()));
    }

    // 2. Overwrite strictly inside (from 2F to 4F)
    PlacedClip pc2;
    pc2.track = track0_id;
    pc2.clip = filler_clip;
    pc2.clip.duration = core::Duration::from_ticks(2 * F);

    OverwriteClips ov2{seq_id, model::TimelineTime::from_ticks(2 * F), {pc2}};
    auto rt_ov2 = test::round_trip(editor, ov2);
    ASSERT_TRUE(is_ok(rt_ov2));
    {
        const auto& trk_clips = editor.snapshot()->sequences.at(seq_id).tracks[0].clips;
        const auto& left = trk_clips[0];
        const auto& right = trk_clips[2];
        EXPECT_EQ(left.transform.opacity.keyframes(), orig_ptr);
        EXPECT_EQ(right.start.ticks(), 4 * F);
        expect_same_motion(orig_clip, right, 4 * F, samples);
        EXPECT_TRUE(is_ok(editor.undo()));
    }

    // 3. InsertClips straddling an animated clip at 3F
    PlacedClip pc_ins;
    pc_ins.track = track0_id;
    pc_ins.clip = filler_clip;
    pc_ins.clip.duration = core::Duration::from_ticks(2 * F);

    InsertClips ins_cmd{
        seq_id, model::TimelineTime::from_ticks(3 * F), {pc_ins}, RippleScope::AllUnlockedTracks};
    auto rt_ins = test::round_trip(editor, ins_cmd);
    ASSERT_TRUE(is_ok(rt_ins));
    {
        const auto& trk_clips = editor.snapshot()->sequences.at(seq_id).tracks[0].clips;
        const auto& left = trk_clips[0];
        const auto& right = trk_clips[2];
        EXPECT_EQ(left.transform.opacity.keyframes(), orig_ptr);
        EXPECT_EQ(right.start.ticks(), 5 * F);
        expect_same_motion(orig_clip, right, 3 * F, samples);
    }
}

// C7: RateStretch tail x2, x1/2, head non-ripple x2, ripple head x2, linked pair
TEST(KeyframeEditTest, C7_RateStretchScaling) {
    core::UuidGenerator gen(9007ULL);
    auto project = model::test::build_valid_project(gen);
    const auto seq_id = project.main_sequence;
    auto& seq = project.sequences.at(seq_id);
    const std::int64_t F = core::frame_duration(seq.frame_rate)->ticks();

    auto& c0 = seq.tracks[0].clips[0];
    c0.start = model::TimelineTime::from_ticks(16 * F);
    c0.duration = core::Duration::from_ticks(8 * F);
    c0.link_id = std::nullopt;
    seq.tracks[1].clips[0].link_id = std::nullopt;
    animate_clip(c0, F);

    const auto orig_clip = c0;
    const auto cid = c0.id;
    const std::vector<std::int64_t> samples{0, F, 2 * F, 4 * F, 6 * F, 8 * F};

    auto ed_res = Editor::create(std::move(project), gen);
    ASSERT_TRUE(is_ok(ed_res));
    Editor& editor = *ed_res.value();

    // 1. Tail x2 (D: 8F -> 16F, new_edge = 32F)
    {
        RateStretch cmd{seq_id,
                        cid,
                        TrimEdge::Tail,
                        model::TimelineTime::from_ticks(32 * F),
                        false,
                        RippleScope::AllUnlockedTracks,
                        true};
        auto rt = test::round_trip(editor, cmd);
        ASSERT_TRUE(is_ok(rt));
        const auto& cl = editor.snapshot()->sequences.at(seq_id).tracks[0].clips[0];
        EXPECT_EQ(cl.duration.ticks(), 16 * F);
        expect_scaled_motion(orig_clip, cl, 2, 1, samples);
        EXPECT_TRUE(is_ok(editor.undo()));
    }

    // 2. Tail x1/2 (D: 8F -> 4F, new_edge = 20F)
    {
        RateStretch cmd{seq_id,
                        cid,
                        TrimEdge::Tail,
                        model::TimelineTime::from_ticks(20 * F),
                        false,
                        RippleScope::AllUnlockedTracks,
                        true};
        auto rt = test::round_trip(editor, cmd);
        ASSERT_TRUE(is_ok(rt));
        const auto& cl = editor.snapshot()->sequences.at(seq_id).tracks[0].clips[0];
        EXPECT_EQ(cl.duration.ticks(), 4 * F);
        expect_scaled_motion(orig_clip, cl, 1, 2, samples);
        EXPECT_TRUE(is_ok(editor.undo()));
    }

    // 3. Head non-ripple x2 (start moved from 16F to 8F, new_edge = 8F, duration becomes 16F)
    {
        RateStretch cmd{seq_id,
                        cid,
                        TrimEdge::Head,
                        model::TimelineTime::from_ticks(8 * F),
                        false,
                        RippleScope::AllUnlockedTracks,
                        true};
        auto rt = test::round_trip(editor, cmd);
        ASSERT_TRUE(is_ok(rt));
        const auto& cl = editor.snapshot()->sequences.at(seq_id).tracks[0].clips[0];
        EXPECT_EQ(cl.start.ticks(), 8 * F);
        EXPECT_EQ(cl.duration.ticks(), 16 * F);
        expect_scaled_motion(orig_clip, cl, 2, 1, samples);
        EXPECT_TRUE(is_ok(editor.undo()));
    }

    // 4. Ripple head x2 (new_edge = 8F, duration becomes 16F)
    {
        RateStretch cmd{seq_id,
                        cid,
                        TrimEdge::Head,
                        model::TimelineTime::from_ticks(8 * F),
                        true,
                        RippleScope::AllUnlockedTracks,
                        true};
        auto rt = test::round_trip(editor, cmd);
        ASSERT_TRUE(is_ok(rt));
        const auto& cl = editor.snapshot()->sequences.at(seq_id).tracks[0].clips[0];
        EXPECT_EQ(cl.duration.ticks(), 16 * F);
        expect_scaled_motion(orig_clip, cl, 2, 1, samples);
        EXPECT_TRUE(is_ok(editor.undo()));
    }
}

// C7b: RateStretch on a linked video + audio pair scales the keyframes of BOTH clips
TEST(KeyframeEditTest, C7b_RateStretchLinkedPair) {
    core::UuidGenerator gen(9017ULL);
    auto project = model::test::build_valid_project(gen);
    const auto seq_id = project.main_sequence;
    auto& seq = project.sequences.at(seq_id);
    const std::int64_t F = core::frame_duration(seq.frame_rate)->ticks();

    auto& v_clip = seq.tracks[0].clips[0];
    auto& a_clip = seq.tracks[1].clips[0];
    ASSERT_TRUE(v_clip.link_id.has_value());
    ASSERT_EQ(v_clip.link_id, a_clip.link_id);
    v_clip.start = model::TimelineTime::zero();
    v_clip.duration = core::Duration::from_ticks(8 * F);
    a_clip.start = model::TimelineTime::zero();
    a_clip.duration = core::Duration::from_ticks(8 * F);
    animate_clip(v_clip, F);
    animate_audio_clip(a_clip, F);

    const auto orig_v = v_clip;
    const auto v_id = v_clip.id;
    const std::vector<std::int64_t> samples{0, F, 2 * F, 4 * F, 6 * F, 8 * F};

    auto ed_res = Editor::create(std::move(project), gen);
    ASSERT_TRUE(is_ok(ed_res));
    Editor& editor = *ed_res.value();

    // Tail x1/2 (8F -> 4F) with linked partners (ignore_links = false)
    RateStretch cmd{seq_id,         v_id,
                    TrimEdge::Tail, model::TimelineTime::from_ticks(4 * F),
                    false,          RippleScope::AllUnlockedTracks,
                    false};
    auto rt = test::round_trip(editor, cmd);
    ASSERT_TRUE(is_ok(rt));

    const auto snap = editor.snapshot();
    const auto& v_after = snap->sequences.at(seq_id).tracks[0].clips[0];
    const auto& a_after = snap->sequences.at(seq_id).tracks[1].clips[0];
    EXPECT_EQ(v_after.duration.ticks(), 4 * F);
    EXPECT_EQ(a_after.duration.ticks(), 4 * F);
    expect_scaled_motion(orig_v, v_after, 1, 2, samples);

    const auto* a_content = std::get_if<model::AudioContent>(&a_after.content);
    ASSERT_NE(a_content, nullptr);
    ASSERT_TRUE(a_content->volume.is_animated());
    auto a_keys = a_content->volume.keyframes()->keys();
    ASSERT_EQ(a_keys.size(), 2u);
    EXPECT_EQ(a_keys[0].time.ticks(), 0);
    EXPECT_EQ(a_keys[0].value, 1.0);
    EXPECT_EQ(a_keys[1].time.ticks(), 4 * F);
    EXPECT_EQ(a_keys[1].value, 0.0);
}

// C8: RateStretch collision: keys {10, 11} on opacity, stretch D=8F -> 2F (1/4)
TEST(KeyframeEditTest, C8_RateStretchCollision) {
    core::UuidGenerator gen(9008ULL);
    auto project = model::test::build_valid_project(gen);
    const auto seq_id = project.main_sequence;
    auto& seq = project.sequences.at(seq_id);
    const std::int64_t F = core::frame_duration(seq.frame_rate)->ticks();

    auto& c0 = seq.tracks[0].clips[0];
    c0.start = model::TimelineTime::zero();
    c0.duration = core::Duration::from_ticks(8 * F);
    c0.link_id = std::nullopt;
    seq.tracks[1].clips[0].link_id = std::nullopt;

    std::vector<keyframes::Keyframe<double>> keys{
        {core::TimePoint::from_ticks(10), 0.1, keyframes::Interpolation::linear()},
        {core::TimePoint::from_ticks(11), 0.9, keyframes::Interpolation::linear()},
    };
    auto track_res = keyframes::KeyframeTrack<double>::create(std::move(keys));
    ASSERT_TRUE(track_res.has_value());
    c0.transform.opacity.set_keyframes(std::move(track_res.value()));

    const auto cid = c0.id;

    auto ed_res = Editor::create(std::move(project), gen);
    ASSERT_TRUE(is_ok(ed_res));
    Editor& editor = *ed_res.value();

    auto before = editor.snapshot();
    const bool can_undo_before = editor.can_undo();

    RateStretch cmd{seq_id,
                    cid,
                    TrimEdge::Tail,
                    model::TimelineTime::from_ticks(2 * F),
                    false,
                    RippleScope::AllUnlockedTracks,
                    true};
    auto exec_res = editor.execute(cmd);
    EXPECT_TRUE(is_error(exec_res, core::ErrorCode::InvalidArgument));
    EXPECT_NE(exec_res.error().message().find("keyframe collision"), std::string::npos);

    EXPECT_TRUE(model::identical(*editor.snapshot(), *before));
    EXPECT_EQ(editor.can_undo(), can_undo_before);
}

// C9: JoinClips: SplitClip then JoinClips -> identical to original; negative tests
TEST(KeyframeEditTest, C9_JoinClipsRoundTripAndNegatives) {
    core::UuidGenerator gen(9009ULL);
    auto project = model::test::build_valid_project(gen);
    const auto seq_id = project.main_sequence;
    auto& seq = project.sequences.at(seq_id);
    const std::int64_t F = core::frame_duration(seq.frame_rate)->ticks();

    auto& c0 = seq.tracks[0].clips[0];
    c0.start = model::TimelineTime::zero();
    c0.duration = core::Duration::from_ticks(16 * F);
    c0.link_id = std::nullopt;
    seq.tracks[1].clips[0].link_id = std::nullopt;
    animate_clip(c0, F);

    const auto orig_clip = c0;
    const auto* orig_op_ptr = c0.transform.opacity.keyframes();
    const auto cid = c0.id;

    auto ed_res = Editor::create(std::move(project), gen);
    ASSERT_TRUE(is_ok(ed_res));
    Editor& editor = *ed_res.value();

    // 1. SplitClip at 4F
    SplitClip split_cmd{seq_id, cid, model::TimelineTime::from_ticks(4 * F), true};
    auto split_res = editor.execute(split_cmd);
    ASSERT_TRUE(is_ok(split_res));

    const auto& v_clips = editor.snapshot()->sequences.at(seq_id).tracks[0].clips;
    const auto left_id = v_clips[0].id;
    const auto right_id = v_clips[1].id;

    // 2. JoinClips {left, right}
    JoinClips join_cmd{seq_id, {{left_id, right_id}}};
    auto rt_join = test::round_trip(editor, join_cmd);
    ASSERT_TRUE(is_ok(rt_join));

    const auto& joined_clip = editor.snapshot()->sequences.at(seq_id).tracks[0].clips[0];
    EXPECT_TRUE(model::identical(joined_clip, orig_clip));
    EXPECT_EQ(joined_clip.transform.opacity.keyframes(), orig_op_ptr);

    // 3. Negative 1: replace right half's opacity track with a different one -> Join fails
    {
        // Redo split by rolling back join and re-splitting
        EXPECT_TRUE(is_ok(editor.undo()));  // now at split state
        const auto& split_clips = editor.snapshot()->sequences.at(seq_id).tracks[0].clips;
        const auto r_id = split_clips[1].id;

        model::TransformProps new_transform = split_clips[1].transform;
        std::vector<keyframes::Keyframe<double>> diff_keys{
            {core::TimePoint::from_ticks(0), 0.99, keyframes::Interpolation::linear()},
        };
        auto diff_track = keyframes::KeyframeTrack<double>::create(std::move(diff_keys));
        ASSERT_TRUE(diff_track.has_value());
        new_transform.opacity.set_keyframes(std::move(diff_track.value()));

        SetClipProperties set_props{seq_id, r_id, {}, {}, {}, new_transform};
        ASSERT_TRUE(is_ok(editor.execute(set_props)));

        JoinClips fail_join{seq_id, {{left_id, r_id}}};
        auto fail_res = editor.execute(fail_join);
        EXPECT_TRUE(is_error(fail_res, core::ErrorCode::InvalidArgument));
        EXPECT_NE(fail_res.error().message().find("not otherwise identical"), std::string::npos);

        EXPECT_TRUE(is_ok(editor.undo()));  // undo SetClipProperties
    }

    // 4. Negative 2: one half animated, the other not -> same failure
    {
        const auto& split_clips = editor.snapshot()->sequences.at(seq_id).tracks[0].clips;
        const auto r_id = split_clips[1].id;

        model::TransformProps non_anim_transform = split_clips[1].transform;
        non_anim_transform.opacity.clear_keyframes();

        SetClipProperties set_props{seq_id, r_id, {}, {}, {}, non_anim_transform};
        ASSERT_TRUE(is_ok(editor.execute(set_props)));

        JoinClips fail_join{seq_id, {{left_id, r_id}}};
        auto fail_res = editor.execute(fail_join);
        EXPECT_TRUE(is_error(fail_res, core::ErrorCode::InvalidArgument));
        EXPECT_NE(fail_res.error().message().find("not otherwise identical"), std::string::npos);

        EXPECT_TRUE(is_ok(editor.undo()));  // undo SetClipProperties
    }

    // 5. Non-animated halves still join
    {
        // Join original split halves
        const auto& split_clips = editor.snapshot()->sequences.at(seq_id).tracks[0].clips;
        JoinClips clean_join{seq_id, {{split_clips[0].id, split_clips[1].id}}};
        EXPECT_TRUE(is_ok(editor.execute(clean_join)));
    }
}

}  // namespace
}  // namespace nxtcut::commands
