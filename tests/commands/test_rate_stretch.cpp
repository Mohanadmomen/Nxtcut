#include <nxtcut/commands/editor.hpp>
#include <nxtcut/commands/timeline_commands.hpp>
#include <nxtcut/core/frame_rate.hpp>
#include <nxtcut/core/time.hpp>
#include <nxtcut/core/uuid.hpp>
#include <nxtcut/model/clip.hpp>
#include <nxtcut/model/equality.hpp>
#include <nxtcut/model/ids.hpp>
#include <nxtcut/model/project.hpp>
#include <nxtcut/model/speed.hpp>
#include <nxtcut/model/time_coords.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "test_helper.hpp"
#include <nxtcut_test/assertions.hpp>
#include <nxtcut_test/model_fixtures.hpp>

namespace nxtcut::commands {
namespace {

constexpr std::int64_t kF = 29'400'000;

inline model::TimelineTime time_f(std::int64_t frames) {
    return model::TimelineTime::from_ticks(frames * kF);
}

inline core::Duration dur_f(std::int64_t frames) {
    return core::Duration::from_ticks(frames * kF);
}

inline model::SourceTime src_f(std::int64_t frames) {
    return model::SourceTime::from_ticks(frames * kF);
}

struct RateStretchFixture {
    model::Project project;
    model::SequenceId seq_id;
    model::TrackId v_track_id;
    model::TrackId a_track_id;
    model::ClipId clip_x_id;
    model::ClipId clip_y_id;
    model::MediaId video_media_id;
    model::MediaId image_media_id;
};

RateStretchFixture create_rate_stretch_fixture(core::UuidGenerator& gen, bool include_y = true,
                                               std::int64_t media_dur_f = 20000) {
    auto project = model::test::build_valid_project(gen);
    const auto seq_id = project.main_sequence;
    auto& seq = project.sequences.at(seq_id);
    seq.frame_rate = core::frame_rates::k24;

    const auto video_media_id = std::get<model::VideoContent>(seq.tracks[0].clips[0].content).media;
    project.media[video_media_id].duration = dur_f(media_dur_f);
    project.media[video_media_id].video->frame_rate = core::frame_rates::k24;

    const auto image_media_id = std::get<model::ImageContent>(seq.tracks[0].clips[1].content).media;

    const auto v_track_id = seq.tracks[0].id;
    const auto a_track_id = seq.tracks[1].id;

    const auto clip_x_id = model::generate_id<model::ClipId>(gen);
    const auto clip_y_id = model::generate_id<model::ClipId>(gen);

    // X [start 100, dur 100, src_in 200, speed 1/1]
    model::Clip x;
    x.id = clip_x_id;
    x.name = "ClipX";
    x.start = time_f(100);
    x.duration = dur_f(100);
    x.source_in = src_f(200);
    x.speed = model::Speed::normal();
    x.content = model::VideoContent{video_media_id};

    seq.tracks[0].clips.clear();
    seq.tracks[0].clips.push_back(x);

    if (include_y) {
        // Y [start 200, dur 100, src_in 500, speed 1/1]
        model::Clip y;
        y.id = clip_y_id;
        y.name = "ClipY";
        y.start = time_f(200);
        y.duration = dur_f(100);
        y.source_in = src_f(500);
        y.speed = model::Speed::normal();
        y.content = model::VideoContent{video_media_id};
        seq.tracks[0].clips.push_back(y);
    }

    seq.tracks[1].clips.clear();

    return RateStretchFixture{std::move(project), seq_id,    v_track_id,     a_track_id,
                              clip_x_id,          clip_y_id, video_media_id, image_media_id};
}

// T1: tail to 250F (X alone): dur 150, speed 2/3, src_in 200 unchanged, start 100.
// source_span equals 100F.
TEST(RateStretchTest, T1_TailTo250F_XAlone) {
    core::UuidGenerator gen(8001ULL);
    auto fix = create_rate_stretch_fixture(gen, false);
    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    RateStretch cmd{fix.seq_id, fix.clip_x_id, TrimEdge::Tail, time_f(250)};
    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));
    EXPECT_TRUE(rt_res.value().created_clips.empty());

    const auto& new_x = editor.snapshot()->sequences.at(fix.seq_id).tracks[0].clips[0];
    EXPECT_EQ(new_x.start, time_f(100));
    EXPECT_EQ(new_x.duration, dur_f(150));
    EXPECT_EQ(new_x.source_in, src_f(200));
    EXPECT_EQ(new_x.speed, model::Speed::create(2, 3).value());
    EXPECT_EQ(model::source_span(new_x).value(), dur_f(100));
}

// T2: tail to 150F: dur 50, speed 2/1.
TEST(RateStretchTest, T2_TailTo150F_XAlone) {
    core::UuidGenerator gen(8002ULL);
    auto fix = create_rate_stretch_fixture(gen, false);
    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    RateStretch cmd{fix.seq_id, fix.clip_x_id, TrimEdge::Tail, time_f(150)};
    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& new_x = editor.snapshot()->sequences.at(fix.seq_id).tracks[0].clips[0];
    EXPECT_EQ(new_x.start, time_f(100));
    EXPECT_EQ(new_x.duration, dur_f(50));
    EXPECT_EQ(new_x.speed, model::Speed::create(2, 1).value());
    EXPECT_EQ(model::source_span(new_x).value(), dur_f(100));
}

// T3: tail to 101F: dur 1F, speed 100/1 (allowed, boundary).
TEST(RateStretchTest, T3_TailTo101F_BoundaryMaxSpeed) {
    core::UuidGenerator gen(8003ULL);
    auto fix = create_rate_stretch_fixture(gen, false);
    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    RateStretch cmd{fix.seq_id, fix.clip_x_id, TrimEdge::Tail, time_f(101)};
    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& new_x = editor.snapshot()->sequences.at(fix.seq_id).tracks[0].clips[0];
    EXPECT_EQ(new_x.start, time_f(100));
    EXPECT_EQ(new_x.duration, dur_f(1));
    EXPECT_EQ(new_x.speed, model::Speed::create(100, 1).value());
    EXPECT_EQ(model::source_span(new_x).value(), dur_f(100));
}

// T4: tail to 10100F: dur 10000F, speed 1/100 (allowed, boundary);
// tail to 10101F: InvalidArgument containing "speed" (project unchanged).
TEST(RateStretchTest, T4_TailBoundaryMinSpeedAndExceeded) {
    core::UuidGenerator gen(8004ULL);
    auto fix = create_rate_stretch_fixture(gen, false, 20000);
    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    // Allowed boundary: tail to 10100F => dur 10000F, speed 1/100
    RateStretch cmd_ok{fix.seq_id, fix.clip_x_id, TrimEdge::Tail, time_f(10100)};
    auto rt_res = test::round_trip(editor, cmd_ok);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& new_x = editor.snapshot()->sequences.at(fix.seq_id).tracks[0].clips[0];
    EXPECT_EQ(new_x.duration, dur_f(10000));
    EXPECT_EQ(new_x.speed, model::Speed::create(1, 100).value());
    EXPECT_EQ(model::source_span(new_x).value(), dur_f(100));

    // Reset back to initial snapshot for 10101F test
    ASSERT_TRUE(test::is_ok(editor.undo()));

    const auto before = editor.snapshot();
    RateStretch cmd_fail{fix.seq_id, fix.clip_x_id, TrimEdge::Tail, time_f(10101)};
    auto fail_res = editor.execute(cmd_fail);
    EXPECT_TRUE(test::is_error(fail_res, core::ErrorCode::InvalidArgument));
    EXPECT_NE(fail_res.error().message().find("speed"), std::string_view::npos);
    EXPECT_TRUE(model::identical(*editor.snapshot(), *before));
}

// T5: head to 50F (X alone, non-ripple): start 50, dur 150, end stays 200, src_in 200, speed 2/3.
// Head to 150F: start 150, dur 50, speed 2/1.
TEST(RateStretchTest, T5_HeadNonRipple) {
    core::UuidGenerator gen(8005ULL);
    auto fix = create_rate_stretch_fixture(gen, false);
    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    // Head to 50F
    RateStretch cmd1{fix.seq_id, fix.clip_x_id, TrimEdge::Head, time_f(50), false};
    auto rt_res1 = test::round_trip(editor, cmd1);
    ASSERT_TRUE(test::is_ok(rt_res1));

    const auto& x1 = editor.snapshot()->sequences.at(fix.seq_id).tracks[0].clips[0];
    EXPECT_EQ(x1.start, time_f(50));
    EXPECT_EQ(x1.duration, dur_f(150));
    EXPECT_EQ(x1.source_in, src_f(200));
    EXPECT_EQ(x1.speed, model::Speed::create(2, 3).value());
    EXPECT_EQ(model::source_span(x1).value(), dur_f(100));

    // Head to 150F from previous state
    RateStretch cmd2{fix.seq_id, fix.clip_x_id, TrimEdge::Head, time_f(150), false};
    auto rt_res2 = test::round_trip(editor, cmd2);
    ASSERT_TRUE(test::is_ok(rt_res2));

    const auto& x2 = editor.snapshot()->sequences.at(fix.seq_id).tracks[0].clips[0];
    EXPECT_EQ(x2.start, time_f(150));
    EXPECT_EQ(x2.duration, dur_f(50));
    EXPECT_EQ(x2.source_in, src_f(200));
    EXPECT_EQ(x2.speed, model::Speed::create(2, 1).value());
    EXPECT_EQ(model::source_span(x2).value(), dur_f(100));
}

// T6: ripple tail to 250F with Y present: X dur 150, Y start 250 (shifted +50).
// Ripple head to 150F: X start stays 100, dur 50, speed 2/1, Y start 150 (shifted -50).
// Ripple with scope EditedTracksOnly leaves other tracks alone.
TEST(RateStretchTest, T6_RippleTailAndHeadAndScope) {
    core::UuidGenerator gen(8006ULL);
    auto fix = create_rate_stretch_fixture(gen, true);

    // Add clip Z [200, 100] on track 1 (Audio track) referencing the same media
    const auto clip_z_id = model::generate_id<model::ClipId>(gen);
    model::Clip z;
    z.id = clip_z_id;
    z.name = "ClipZ";
    z.start = time_f(200);
    z.duration = dur_f(100);
    z.source_in = src_f(0);
    z.speed = model::Speed::normal();
    z.content = model::AudioContent{fix.video_media_id};
    fix.project.sequences.at(fix.seq_id).tracks[1].clips.push_back(z);

    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    // 1. Ripple tail to 250F: X dur 150, Y start 250 (shifted +50).
    RateStretch cmd_tail{fix.seq_id, fix.clip_x_id, TrimEdge::Tail, time_f(250), true};
    auto rt_res = test::round_trip(editor, cmd_tail);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& v_clips = editor.snapshot()->sequences.at(fix.seq_id).tracks[0].clips;
    EXPECT_EQ(v_clips[0].duration, dur_f(150));
    EXPECT_EQ(v_clips[1].start, time_f(250));

    // Restore to initial state
    ASSERT_TRUE(test::is_ok(editor.undo()));

    // 2. Ripple head to 150F: X start stays 100, dur 50, speed 2/1, Y start 150 (shifted -50).
    RateStretch cmd_head{fix.seq_id, fix.clip_x_id, TrimEdge::Head, time_f(150), true};
    auto rt_res_head = test::round_trip(editor, cmd_head);
    ASSERT_TRUE(test::is_ok(rt_res_head));

    const auto& v_clips_head = editor.snapshot()->sequences.at(fix.seq_id).tracks[0].clips;
    EXPECT_EQ(v_clips_head[0].start, time_f(100));
    EXPECT_EQ(v_clips_head[0].duration, dur_f(50));
    EXPECT_EQ(v_clips_head[0].speed, model::Speed::create(2, 1).value());
    EXPECT_EQ(v_clips_head[1].start, time_f(150));

    // Restore to initial state
    ASSERT_TRUE(test::is_ok(editor.undo()));

    // 3. Ripple with scope EditedTracksOnly leaves other tracks alone:
    RateStretch cmd_scope{fix.seq_id,  fix.clip_x_id, TrimEdge::Tail,
                          time_f(250), true,          RippleScope::EditedTracksOnly};
    auto rt_res_scope = test::round_trip(editor, cmd_scope);
    ASSERT_TRUE(test::is_ok(rt_res_scope));

    const auto& v_clips_sc = editor.snapshot()->sequences.at(fix.seq_id).tracks[0].clips;
    const auto& a_clips_sc = editor.snapshot()->sequences.at(fix.seq_id).tracks[1].clips;
    EXPECT_EQ(v_clips_sc[0].duration, dur_f(150));
    EXPECT_EQ(v_clips_sc[1].start, time_f(250));
    // Z on Audio track was untouched
    EXPECT_EQ(a_clips_sc[0].start, time_f(200));
}

// T7: non-ripple tail extension into Y (tail to 250F with Y at 200F): InvalidArgument "overlap".
TEST(RateStretchTest, T7_NonRippleTailExtensionIntoY_FailsWithOverlap) {
    core::UuidGenerator gen(8007ULL);
    auto fix = create_rate_stretch_fixture(gen, true);
    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto before = editor.snapshot();
    RateStretch cmd{fix.seq_id, fix.clip_x_id, TrimEdge::Tail, time_f(250), false};
    auto res = editor.execute(cmd);
    EXPECT_TRUE(test::is_error(res, core::ErrorCode::InvalidArgument));
    EXPECT_NE(res.error().message().find("overlap"), std::string_view::npos);
    EXPECT_TRUE(model::identical(*editor.snapshot(), *before));
}

// T8: no-op: tail to 200F: empty ChangeSet.
// Sub-frame: 250F + 14,699,999 ticks acts as 250F; 250F + 14,700,000 ticks acts as 251F.
// Head/tail making duration 0: InvalidArgument; duration under one frame: "one frame".
TEST(RateStretchTest, T8_NoOpAndSubFrameAndZeroDuration) {
    core::UuidGenerator gen(8008ULL);
    auto fix = create_rate_stretch_fixture(gen, false);
    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    // No-op: tail to 200F (current end)
    RateStretch cmd_noop{fix.seq_id, fix.clip_x_id, TrimEdge::Tail, time_f(200)};
    auto res_noop = editor.execute(cmd_noop);
    ASSERT_TRUE(test::is_ok(res_noop));
    EXPECT_TRUE(res_noop.value().created_clips.empty());

    // Sub-frame: 250F + 14,699,999 ticks acts as 250F
    const auto t_below = model::TimelineTime::from_ticks(250 * kF + 14'699'999);
    RateStretch cmd_below{fix.seq_id, fix.clip_x_id, TrimEdge::Tail, t_below};
    auto rt_below = test::round_trip(editor, cmd_below);
    ASSERT_TRUE(test::is_ok(rt_below));
    EXPECT_EQ(editor.snapshot()->sequences.at(fix.seq_id).tracks[0].clips[0].duration, dur_f(150));
    ASSERT_TRUE(test::is_ok(editor.undo()));

    // Sub-frame: 250F + 14,700,000 ticks acts as 251F
    const auto t_at = model::TimelineTime::from_ticks(250 * kF + 14'700'000);
    RateStretch cmd_at{fix.seq_id, fix.clip_x_id, TrimEdge::Tail, t_at};
    auto rt_at = test::round_trip(editor, cmd_at);
    ASSERT_TRUE(test::is_ok(rt_at));
    EXPECT_EQ(editor.snapshot()->sequences.at(fix.seq_id).tracks[0].clips[0].duration, dur_f(151));
    ASSERT_TRUE(test::is_ok(editor.undo()));

    // Tail making duration 0: tail to 100F
    RateStretch cmd_tail_zero{fix.seq_id, fix.clip_x_id, TrimEdge::Tail, time_f(100)};
    auto res_tail_zero = editor.execute(cmd_tail_zero);
    EXPECT_TRUE(test::is_error(res_tail_zero, core::ErrorCode::InvalidArgument));

    // Head making duration 0: head to 200F
    RateStretch cmd_head_zero{fix.seq_id, fix.clip_x_id, TrimEdge::Head, time_f(200)};
    auto res_head_zero = editor.execute(cmd_head_zero);
    EXPECT_TRUE(test::is_error(res_head_zero, core::ErrorCode::InvalidArgument));

    // Tail making duration under one frame: duration < 1 frame triggers "one frame"
    // Use an unaligned start clip to test duration under one frame
    auto fix_under = create_rate_stretch_fixture(gen, false);
    fix_under.project.sequences.at(fix_under.seq_id).tracks[0].clips[0].start =
        model::TimelineTime::from_ticks(100 * kF + 14'700'000);
    auto ed_under_res = Editor::create(std::move(fix_under.project), gen);
    ASSERT_TRUE(test::is_ok(ed_under_res));
    Editor& editor_under = *ed_under_res.value();
    RateStretch cmd_under_frame{fix_under.seq_id, fix_under.clip_x_id, TrimEdge::Tail, time_f(101)};
    auto res_under = editor_under.execute(cmd_under_frame);
    EXPECT_TRUE(test::is_error(res_under, core::ErrorCode::InvalidArgument));
    EXPECT_NE(res_under.error().message().find("one frame"), std::string_view::npos);
}

// T9: X with speed 2/1, dur 100, src_in 200 (span 200F): tail to 250F gives dur 150, speed 4/3.
// new source_span still 200F.
TEST(RateStretchTest, T9_InitialSpeed2xTailTo250F) {
    core::UuidGenerator gen(8009ULL);
    auto fix = create_rate_stretch_fixture(gen, false);
    // Set X to speed 2/1 (span 200F)
    fix.project.sequences.at(fix.seq_id).tracks[0].clips[0].speed =
        model::Speed::create(2, 1).value();

    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    RateStretch cmd{fix.seq_id, fix.clip_x_id, TrimEdge::Tail, time_f(250)};
    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& new_x = editor.snapshot()->sequences.at(fix.seq_id).tracks[0].clips[0];
    EXPECT_EQ(new_x.duration, dur_f(150));
    EXPECT_EQ(new_x.speed, model::Speed::create(4, 3).value());
    EXPECT_EQ(model::source_span(new_x).value(), dur_f(200));
}

// T10: speed 1/11 clip (Speed::create(1, 11)), dur 100F: for tails to 130F, 150F and 175F offsets,
// the new clip's model::source_span equals the OLD source_span in every case.
TEST(RateStretchTest, T10_Speed1Over11ClipPreservesSourceSpan) {
    core::UuidGenerator gen(8010ULL);
    auto fix = create_rate_stretch_fixture(gen, false);
    fix.project.sequences.at(fix.seq_id).tracks[0].clips[0].speed =
        model::Speed::create(1, 11).value();

    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto old_span =
        model::source_span(editor.snapshot()->sequences.at(fix.seq_id).tracks[0].clips[0]).value();

    // Offsets 130F, 150F, 175F relative to start 100F => new tails 230F, 250F, 275F
    for (const auto offset : {130, 150, 175}) {
        RateStretch cmd{fix.seq_id, fix.clip_x_id, TrimEdge::Tail, time_f(100 + offset)};
        auto rt_res = test::round_trip(editor, cmd);
        ASSERT_TRUE(test::is_ok(rt_res));

        const auto& cl = editor.snapshot()->sequences.at(fix.seq_id).tracks[0].clips[0];
        EXPECT_EQ(model::source_span(cl).value(), old_span);

        // Reset for next test
        ASSERT_TRUE(test::is_ok(editor.undo()));
    }
}

// T11: linked: audio A' [100, 100, src_in 200] linked to X. Tail stretch of X to 250F:
// A' dur 150 speed 2/3 too. ignore_links: only X changes.
// A partner that would break the speed limit makes the whole command fail (project unchanged).
TEST(RateStretchTest, T11_LinkedPartnersAndIgnoreLinksAndPartnerFailure) {
    core::UuidGenerator gen(8011ULL);
    auto fix = create_rate_stretch_fixture(gen, false);

    const auto link_id = model::generate_id<model::LinkId>(gen);
    fix.project.sequences.at(fix.seq_id).tracks[0].clips[0].link_id = link_id;

    const auto clip_a_prime_id = model::generate_id<model::ClipId>(gen);
    model::Clip a_prime;
    a_prime.id = clip_a_prime_id;
    a_prime.name = "ClipAPrime";
    a_prime.start = time_f(100);
    a_prime.duration = dur_f(100);
    a_prime.source_in = src_f(200);
    a_prime.speed = model::Speed::normal();
    a_prime.link_id = link_id;
    a_prime.content = model::AudioContent{fix.video_media_id};
    fix.project.sequences.at(fix.seq_id).tracks[1].clips.push_back(a_prime);

    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    // 1. Linked tail stretch to 250F: both X and A' stretch to 150F, speed 2/3
    RateStretch cmd{fix.seq_id,  fix.clip_x_id, TrimEdge::Tail,
                    time_f(250), false,         RippleScope::AllUnlockedTracks,
                    false};
    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& new_x = editor.snapshot()->sequences.at(fix.seq_id).tracks[0].clips[0];
    const auto& new_a = editor.snapshot()->sequences.at(fix.seq_id).tracks[1].clips[0];
    EXPECT_EQ(new_x.duration, dur_f(150));
    EXPECT_EQ(new_x.speed, model::Speed::create(2, 3).value());
    EXPECT_EQ(new_a.duration, dur_f(150));
    EXPECT_EQ(new_a.speed, model::Speed::create(2, 3).value());

    // Restore to initial state
    ASSERT_TRUE(test::is_ok(editor.undo()));

    // 2. ignore_links: only X changes, A' stays 100F, speed 1/1
    RateStretch cmd_ignore{fix.seq_id,  fix.clip_x_id, TrimEdge::Tail,
                           time_f(250), false,         RippleScope::AllUnlockedTracks,
                           true};
    auto rt_ignore = test::round_trip(editor, cmd_ignore);
    ASSERT_TRUE(test::is_ok(rt_ignore));

    const auto& x_ign = editor.snapshot()->sequences.at(fix.seq_id).tracks[0].clips[0];
    const auto& a_ign = editor.snapshot()->sequences.at(fix.seq_id).tracks[1].clips[0];
    EXPECT_EQ(x_ign.duration, dur_f(150));
    EXPECT_EQ(a_ign.duration, dur_f(100));
    EXPECT_EQ(a_ign.speed, model::Speed::normal());

    // Restore to initial state
    ASSERT_TRUE(test::is_ok(editor.undo()));

    // 3. Partner that would break speed limit: set A' speed to 100/1, span = 10000F.
    // Stretching tail of X to 101F would make A' duration 1F, requiring speed 10000/1 (> 100/1).
    auto fix_pf = create_rate_stretch_fixture(gen, false);
    const auto link_pf_id = model::generate_id<model::LinkId>(gen);
    fix_pf.project.sequences.at(fix_pf.seq_id).tracks[0].clips[0].link_id = link_pf_id;

    const auto clip_pf_a_id = model::generate_id<model::ClipId>(gen);
    model::Clip a_pf;
    a_pf.id = clip_pf_a_id;
    a_pf.name = "ClipAPFail";
    a_pf.start = time_f(100);
    a_pf.duration = dur_f(100);
    a_pf.source_in = src_f(0);
    a_pf.speed = model::Speed::create(100, 1).value();
    a_pf.link_id = link_pf_id;
    a_pf.content = model::AudioContent{fix_pf.video_media_id};
    fix_pf.project.sequences.at(fix_pf.seq_id).tracks[1].clips.push_back(a_pf);

    auto ed_pf_res = Editor::create(std::move(fix_pf.project), gen);
    ASSERT_TRUE(test::is_ok(ed_pf_res));
    Editor& editor_pf = *ed_pf_res.value();
    const auto before = editor_pf.snapshot();

    RateStretch cmd_partner_fail{fix_pf.seq_id, fix_pf.clip_x_id, TrimEdge::Tail, time_f(101)};
    auto fail_res = editor_pf.execute(cmd_partner_fail);
    EXPECT_TRUE(test::is_error(fail_res, core::ErrorCode::InvalidArgument));
    EXPECT_NE(fail_res.error().message().find("speed"), std::string_view::npos);
    EXPECT_TRUE(model::identical(*editor_pf.snapshot(), *before));
}

// T12: Image and Text clips (primary or partner): InvalidArgument containing "stretch".
// Unknown clip: NotFound. Locked track: "locked".
TEST(RateStretchTest, T12_InvalidClipsAndUnknownAndLocked) {
    core::UuidGenerator gen(8012ULL);
    auto fix = create_rate_stretch_fixture(gen, false);

    // Add Image clip on track 0
    const auto image_clip_id = model::generate_id<model::ClipId>(gen);
    model::Clip img;
    img.id = image_clip_id;
    img.name = "ImageClip";
    img.start = time_f(300);
    img.duration = dur_f(100);
    img.source_in = model::SourceTime::zero();
    img.speed = model::Speed::normal();
    img.content = model::ImageContent{fix.image_media_id};
    fix.project.sequences.at(fix.seq_id).tracks[0].clips.push_back(img);

    // Add Text clip on track 0
    const auto text_clip_id = model::generate_id<model::ClipId>(gen);
    model::Clip txt;
    txt.id = text_clip_id;
    txt.name = "TextClip";
    txt.start = time_f(450);
    txt.duration = dur_f(100);
    txt.source_in = model::SourceTime::zero();
    txt.speed = model::Speed::normal();
    txt.content = model::TextContent{"Title"};
    fix.project.sequences.at(fix.seq_id).tracks[0].clips.push_back(txt);

    // Primary or partner Image/Text clips
    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    // Rate stretch Image clip fails
    RateStretch cmd_img{fix.seq_id, image_clip_id, TrimEdge::Tail, time_f(450)};
    auto res_img = editor.execute(cmd_img);
    EXPECT_TRUE(test::is_error(res_img, core::ErrorCode::InvalidArgument));
    EXPECT_NE(res_img.error().message().find("stretch"), std::string_view::npos);

    // Rate stretch Text clip fails
    RateStretch cmd_txt{fix.seq_id, text_clip_id, TrimEdge::Tail, time_f(600)};
    auto res_txt = editor.execute(cmd_txt);
    EXPECT_TRUE(test::is_error(res_txt, core::ErrorCode::InvalidArgument));
    EXPECT_NE(res_txt.error().message().find("stretch"), std::string_view::npos);

    // Unknown clip fails with NotFound
    const auto unknown_id = model::generate_id<model::ClipId>(gen);
    RateStretch cmd_unknown{fix.seq_id, unknown_id, TrimEdge::Tail, time_f(250)};
    auto res_unk = editor.execute(cmd_unknown);
    EXPECT_TRUE(test::is_error(res_unk, core::ErrorCode::NotFound));

    // Locked track fails with "locked"
    auto fix_locked = create_rate_stretch_fixture(gen, false);
    fix_locked.project.sequences.at(fix_locked.seq_id).tracks[0].locked = true;
    auto ed_locked_res = Editor::create(std::move(fix_locked.project), gen);
    ASSERT_TRUE(test::is_ok(ed_locked_res));
    Editor& editor_locked = *ed_locked_res.value();
    RateStretch cmd_locked{fix_locked.seq_id, fix_locked.clip_x_id, TrimEdge::Tail, time_f(250)};
    auto res_locked = editor_locked.execute(cmd_locked);
    EXPECT_TRUE(test::is_error(res_locked, core::ErrorCode::InvalidArgument));
    EXPECT_NE(res_locked.error().message().find("locked"), std::string_view::npos);
}

// T13: audio fades are fitted when a stretch shortens an audio clip
// (fade_in 60F, fade_out 40F on a 100F clip, tail to 190F => dur 90: fade_in 60, fade_out 30).
TEST(RateStretchTest, T13_AudioFadesFittedOnShorten) {
    core::UuidGenerator gen(8013ULL);
    auto fix = create_rate_stretch_fixture(gen, false);

    const auto audio_clip_id = model::generate_id<model::ClipId>(gen);
    model::AudioContent content{fix.video_media_id};
    content.fade_in = dur_f(60);
    content.fade_out = dur_f(40);

    model::Clip a;
    a.id = audio_clip_id;
    a.name = "AudioWithFades";
    a.start = time_f(100);
    a.duration = dur_f(100);
    a.source_in = src_f(0);
    a.speed = model::Speed::normal();
    a.content = content;
    fix.project.sequences.at(fix.seq_id).tracks[1].clips.push_back(a);

    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    RateStretch cmd{fix.seq_id, audio_clip_id, TrimEdge::Tail, time_f(190)};
    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& new_a = editor.snapshot()->sequences.at(fix.seq_id).tracks[1].clips[0];
    EXPECT_EQ(new_a.duration, dur_f(90));
    const auto& new_content = std::get<model::AudioContent>(new_a.content);
    EXPECT_EQ(new_content.fade_in, dur_f(60));
    EXPECT_EQ(new_content.fade_out, dur_f(30));
}

// T14: created_clips is empty.
TEST(RateStretchTest, T14_CreatedClipsIsEmpty) {
    core::UuidGenerator gen(8014ULL);
    auto fix = create_rate_stretch_fixture(gen, false);
    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    RateStretch cmd{fix.seq_id, fix.clip_x_id, TrimEdge::Tail, time_f(250)};
    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));
    EXPECT_TRUE(rt_res.value().created_clips.empty());
}

// T15: helper-level property through the command:
// for stretches on a speed 3/7 clip, source_span before == after.
TEST(RateStretchTest, T15_Speed3Over7PreservesSourceSpan) {
    core::UuidGenerator gen(8015ULL);
    auto fix = create_rate_stretch_fixture(gen, false);
    fix.project.sequences.at(fix.seq_id).tracks[0].clips[0].speed =
        model::Speed::create(3, 7).value();

    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto old_span =
        model::source_span(editor.snapshot()->sequences.at(fix.seq_id).tracks[0].clips[0]).value();

    RateStretch cmd{fix.seq_id, fix.clip_x_id, TrimEdge::Tail, time_f(220)};
    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& new_x = editor.snapshot()->sequences.at(fix.seq_id).tracks[0].clips[0];
    EXPECT_EQ(model::source_span(new_x).value(), old_span);
}

}  // namespace
}  // namespace nxtcut::commands
