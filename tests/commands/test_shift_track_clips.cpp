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

struct ShiftTrackFixture {
    model::Project project;
    model::SequenceId seq_id;
    model::TrackId v_track_id;
    model::TrackId a_track_id;
    model::ClipId clip_a_id;
    model::ClipId clip_b_id;
    model::ClipId clip_c_id;
    model::ClipId clip_b_prime_id;
    model::MediaId video_media_id;
};

ShiftTrackFixture create_shift_fixture(core::UuidGenerator& gen, std::int64_t media_dur_f = 1000) {
    auto project = model::test::build_valid_project(gen);
    const auto seq_id = project.main_sequence;
    auto& seq = project.sequences.at(seq_id);
    seq.frame_rate = core::frame_rates::k24;

    const auto video_media_id = std::get<model::VideoContent>(seq.tracks[0].clips[0].content).media;
    project.media[video_media_id].duration = dur_f(media_dur_f);
    project.media[video_media_id].video->frame_rate = core::frame_rates::k24;

    const auto v_track_id = seq.tracks[0].id;
    const auto a_track_id = seq.tracks[1].id;

    const auto clip_a_id = model::generate_id<model::ClipId>(gen);
    const auto clip_b_id = model::generate_id<model::ClipId>(gen);
    const auto clip_c_id = model::generate_id<model::ClipId>(gen);
    const auto clip_b_prime_id = model::generate_id<model::ClipId>(gen);
    const auto link_b_id = model::generate_id<model::LinkId>(gen);

    // A [0, 50]
    model::Clip a;
    a.id = clip_a_id;
    a.name = "ClipA";
    a.start = time_f(0);
    a.duration = dur_f(50);
    a.source_in = src_f(0);
    a.speed = model::Speed::normal();
    a.content = model::VideoContent{video_media_id};

    // B [100, 50] linked to B'
    model::Clip b;
    b.id = clip_b_id;
    b.name = "ClipB";
    b.start = time_f(100);
    b.duration = dur_f(50);
    b.source_in = src_f(0);
    b.speed = model::Speed::normal();
    b.link_id = link_b_id;
    b.content = model::VideoContent{video_media_id};

    // C [200, 50]
    model::Clip c;
    c.id = clip_c_id;
    c.name = "ClipC";
    c.start = time_f(200);
    c.duration = dur_f(50);
    c.source_in = src_f(0);
    c.speed = model::Speed::normal();
    c.content = model::VideoContent{video_media_id};

    seq.tracks[0].clips.clear();
    seq.tracks[0].clips.push_back(a);
    seq.tracks[0].clips.push_back(b);
    seq.tracks[0].clips.push_back(c);

    // B' [100, 50] on audio track A1 linked to B
    model::Clip b_prime;
    b_prime.id = clip_b_prime_id;
    b_prime.name = "ClipBPrime";
    b_prime.start = time_f(100);
    b_prime.duration = dur_f(50);
    b_prime.source_in = src_f(0);
    b_prime.speed = model::Speed::normal();
    b_prime.link_id = link_b_id;
    b_prime.content = model::AudioContent{video_media_id};

    seq.tracks[1].clips.clear();
    seq.tracks[1].clips.push_back(b_prime);

    return ShiftTrackFixture{std::move(project), seq_id,          v_track_id,
                             a_track_id,         clip_a_id,       clip_b_id,
                             clip_c_id,          clip_b_prime_id, video_media_id};
}

// H1: at 100F, delta +30F: B start 130, C start 230, B' start 130 (partner), A unchanged.
TEST(ShiftTrackClipsTest, H1_At100F_DeltaPlus30F) {
    core::UuidGenerator gen(9001ULL);
    auto fix = create_shift_fixture(gen);
    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    ShiftTrackClips cmd{fix.seq_id, fix.v_track_id, time_f(100), dur_f(30)};
    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));
    EXPECT_TRUE(rt_res.value().created_clips.empty());

    const auto& v_clips = editor.snapshot()->sequences.at(fix.seq_id).tracks[0].clips;
    const auto& a_clips = editor.snapshot()->sequences.at(fix.seq_id).tracks[1].clips;

    EXPECT_EQ(v_clips[0].start, time_f(0));    // A unchanged
    EXPECT_EQ(v_clips[1].start, time_f(130));  // B start 130
    EXPECT_EQ(v_clips[2].start, time_f(230));  // C start 230
    EXPECT_EQ(a_clips[0].start, time_f(130));  // B' start 130
}

// H2: at 100F, delta -30F: B 70, C 170, B' 70.
TEST(ShiftTrackClipsTest, H2_At100F_DeltaMinus30F) {
    core::UuidGenerator gen(9002ULL);
    auto fix = create_shift_fixture(gen);
    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    ShiftTrackClips cmd{fix.seq_id, fix.v_track_id, time_f(100), dur_f(-30)};
    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& v_clips = editor.snapshot()->sequences.at(fix.seq_id).tracks[0].clips;
    const auto& a_clips = editor.snapshot()->sequences.at(fix.seq_id).tracks[1].clips;

    EXPECT_EQ(v_clips[0].start, time_f(0));    // A unchanged
    EXPECT_EQ(v_clips[1].start, time_f(70));   // B start 70
    EXPECT_EQ(v_clips[2].start, time_f(170));  // C start 170
    EXPECT_EQ(a_clips[0].start, time_f(70));   // B' start 70
}

// H3: at 100F, delta -60F: InvalidArgument "overlap" (B would start at 40 over A [0,50)).
// Project unchanged.
TEST(ShiftTrackClipsTest, H3_At100F_DeltaMinus60F_FailsOverlap) {
    core::UuidGenerator gen(9003ULL);
    auto fix = create_shift_fixture(gen);
    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto before = editor.snapshot();
    ShiftTrackClips cmd{fix.seq_id, fix.v_track_id, time_f(100), dur_f(-60)};
    auto res = editor.execute(cmd);
    EXPECT_TRUE(test::is_error(res, core::ErrorCode::InvalidArgument));
    EXPECT_NE(res.error().message().find("overlap"), std::string_view::npos);
    EXPECT_TRUE(model::identical(*editor.snapshot(), *before));
}

// H4: at 0F, delta -1F: InvalidArgument containing "negative".
TEST(ShiftTrackClipsTest, H4_At0F_DeltaMinus1F_FailsNegative) {
    core::UuidGenerator gen(9004ULL);
    auto fix = create_shift_fixture(gen);
    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto before = editor.snapshot();
    ShiftTrackClips cmd{fix.seq_id, fix.v_track_id, time_f(0), dur_f(-1)};
    auto res = editor.execute(cmd);
    EXPECT_TRUE(test::is_error(res, core::ErrorCode::InvalidArgument));
    EXPECT_NE(res.error().message().find("negative"), std::string_view::npos);
    EXPECT_TRUE(model::identical(*editor.snapshot(), *before));
}

// H5: at 101F, delta +30F: only C shifts to 230 (B starts before `at`); B' untouched.
TEST(ShiftTrackClipsTest, H5_At101F_DeltaPlus30F) {
    core::UuidGenerator gen(9005ULL);
    auto fix = create_shift_fixture(gen);
    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    ShiftTrackClips cmd{fix.seq_id, fix.v_track_id, time_f(101), dur_f(30)};
    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& v_clips = editor.snapshot()->sequences.at(fix.seq_id).tracks[0].clips;
    const auto& a_clips = editor.snapshot()->sequences.at(fix.seq_id).tracks[1].clips;

    EXPECT_EQ(v_clips[0].start, time_f(0));    // A unchanged
    EXPECT_EQ(v_clips[1].start, time_f(100));  // B untouched
    EXPECT_EQ(v_clips[2].start, time_f(230));  // C shifts to 230
    EXPECT_EQ(a_clips[0].start, time_f(100));  // B' untouched
}

// H6: at 201F: no clips shift: empty ChangeSet. delta 0: empty.
// delta 14,699,999 ticks: empty. delta 14,700,000 ticks acts as +1F; -14,700,000 acts as -1F.
TEST(ShiftTrackClipsTest, H6_EmptyShiftsAndSubFrameDeltas) {
    core::UuidGenerator gen(9006ULL);
    auto fix = create_shift_fixture(gen);
    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    // At 201F: no clips start >= 201F => empty ChangeSet
    ShiftTrackClips cmd_201{fix.seq_id, fix.v_track_id, time_f(201), dur_f(30)};
    auto res_201 = editor.execute(cmd_201);
    ASSERT_TRUE(test::is_ok(res_201));
    EXPECT_TRUE(res_201.value().created_clips.empty());

    // delta 0 => empty ChangeSet
    ShiftTrackClips cmd_zero{fix.seq_id, fix.v_track_id, time_f(100), core::Duration::zero()};
    auto res_zero = editor.execute(cmd_zero);
    ASSERT_TRUE(test::is_ok(res_zero));

    // delta 14,699,999 ticks snaps to 0 => empty ChangeSet
    ShiftTrackClips cmd_sub_zero{fix.seq_id, fix.v_track_id, time_f(100),
                                 core::Duration::from_ticks(14'699'999)};
    auto res_sub_zero = editor.execute(cmd_sub_zero);
    ASSERT_TRUE(test::is_ok(res_sub_zero));

    // delta 14,700,000 ticks acts as +1F
    ShiftTrackClips cmd_plus1{fix.seq_id, fix.v_track_id, time_f(100),
                              core::Duration::from_ticks(14'700'000)};
    auto rt_plus1 = test::round_trip(editor, cmd_plus1);
    ASSERT_TRUE(test::is_ok(rt_plus1));
    EXPECT_EQ(editor.snapshot()->sequences.at(fix.seq_id).tracks[0].clips[1].start, time_f(101));
    ASSERT_TRUE(test::is_ok(editor.undo()));

    // delta -14,700,000 ticks acts as -1F
    ShiftTrackClips cmd_minus1{fix.seq_id, fix.v_track_id, time_f(100),
                               core::Duration::from_ticks(-14'700'000)};
    auto rt_minus1 = test::round_trip(editor, cmd_minus1);
    ASSERT_TRUE(test::is_ok(rt_minus1));
    EXPECT_EQ(editor.snapshot()->sequences.at(fix.seq_id).tracks[0].clips[1].start, time_f(99));
    ASSERT_TRUE(test::is_ok(editor.undo()));
}

// H7: ignore_links: only the V1 clips shift; B' stays.
TEST(ShiftTrackClipsTest, H7_IgnoreLinks) {
    core::UuidGenerator gen(9007ULL);
    auto fix = create_shift_fixture(gen);
    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    ShiftTrackClips cmd{fix.seq_id, fix.v_track_id, time_f(100), dur_f(30), true};
    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& v_clips = editor.snapshot()->sequences.at(fix.seq_id).tracks[0].clips;
    const auto& a_clips = editor.snapshot()->sequences.at(fix.seq_id).tracks[1].clips;

    EXPECT_EQ(v_clips[1].start, time_f(130));  // B shifted
    EXPECT_EQ(v_clips[2].start, time_f(230));  // C shifted
    EXPECT_EQ(a_clips[0].start, time_f(100));  // B' stayed at 100
}

// H8: partner that starts BEFORE `at`: audio B'' [90F, 60F] linked to B; at 100F delta +10F:
// B and B'' both move +10F.
TEST(ShiftTrackClipsTest, H8_PartnerStartsBeforeAt) {
    core::UuidGenerator gen(9008ULL);
    auto fix = create_shift_fixture(gen);

    // Replace B' with B'' [90F, 60F] linked to B
    const auto link_id = *fix.project.sequences.at(fix.seq_id).tracks[0].clips[1].link_id;
    auto& b_double_prime = fix.project.sequences.at(fix.seq_id).tracks[1].clips[0];
    b_double_prime.start = time_f(90);
    b_double_prime.duration = dur_f(60);
    b_double_prime.link_id = link_id;

    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    ShiftTrackClips cmd{fix.seq_id, fix.v_track_id, time_f(100), dur_f(10)};
    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& v_clips = editor.snapshot()->sequences.at(fix.seq_id).tracks[0].clips;
    const auto& a_clips = editor.snapshot()->sequences.at(fix.seq_id).tracks[1].clips;

    EXPECT_EQ(v_clips[1].start, time_f(110));  // B moved +10F
    EXPECT_EQ(a_clips[0].start, time_f(100));  // B'' moved +10F (from 90 to 100)
}

// H9: a partner that is itself in the shifted set moves exactly once
// (at 0F, delta +10F: B and B' each +10F, not +20F).
TEST(ShiftTrackClipsTest, H9_PartnerInShiftedSetMovesExactlyOnce) {
    core::UuidGenerator gen(9009ULL);
    auto fix = create_shift_fixture(gen);
    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    ShiftTrackClips cmd{fix.seq_id, fix.v_track_id, time_f(0), dur_f(10)};
    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& v_clips = editor.snapshot()->sequences.at(fix.seq_id).tracks[0].clips;
    const auto& a_clips = editor.snapshot()->sequences.at(fix.seq_id).tracks[1].clips;

    EXPECT_EQ(v_clips[0].start, time_f(10));   // A moved +10F
    EXPECT_EQ(v_clips[1].start, time_f(110));  // B moved +10F (not +20F)
    EXPECT_EQ(v_clips[2].start, time_f(210));  // C moved +10F
    EXPECT_EQ(a_clips[0].start, time_f(110));  // B' moved +10F (not +20F)
}

// H10: a pull that collides on the PARTNER's track (put an audio clip right before B')
// fails with "overlap".
TEST(ShiftTrackClipsTest, H10_PullCollidesOnPartnerTrack_FailsOverlap) {
    core::UuidGenerator gen(9010ULL);
    auto fix = create_shift_fixture(gen);

    // Add audio clip A' [0, 90F] on audio track right before B' [100, 50F]
    const auto clip_a_audio_id = model::generate_id<model::ClipId>(gen);
    model::Clip a_audio;
    a_audio.id = clip_a_audio_id;
    a_audio.name = "ClipAAudio";
    a_audio.start = time_f(0);
    a_audio.duration = dur_f(90);
    a_audio.source_in = src_f(0);
    a_audio.speed = model::Speed::normal();
    a_audio.content = model::AudioContent{fix.video_media_id};
    fix.project.sequences.at(fix.seq_id)
        .tracks[1]
        .clips.insert(fix.project.sequences.at(fix.seq_id).tracks[1].clips.begin(), a_audio);

    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto before = editor.snapshot();
    // Pull from 100F by -20F: on V1, B moves to 80F (A on V1 is [0, 50), so no V1 collision),
    // but on A1, B' moves from 100F to 80F, colliding with A' [0, 90)!
    ShiftTrackClips cmd{fix.seq_id, fix.v_track_id, time_f(100), dur_f(-20)};
    auto res = editor.execute(cmd);
    EXPECT_TRUE(test::is_error(res, core::ErrorCode::InvalidArgument));
    EXPECT_NE(res.error().message().find("overlap"), std::string_view::npos);
    EXPECT_TRUE(model::identical(*editor.snapshot(), *before));
}

// H11: locked target track: "locked". Locked partner track: "locked", project unchanged.
// Unknown track or sequence: NotFound. snap(at) < 0: InvalidArgument.
TEST(ShiftTrackClipsTest, H11_LockedTracksAndUnknownAndNegativeAt) {
    core::UuidGenerator gen(9011ULL);
    auto fix = create_shift_fixture(gen);
    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    // 1. Locked target track
    auto fix_target_locked = create_shift_fixture(gen);
    fix_target_locked.project.sequences.at(fix_target_locked.seq_id).tracks[0].locked = true;
    auto ed_tl_res = Editor::create(std::move(fix_target_locked.project), gen);
    ASSERT_TRUE(test::is_ok(ed_tl_res));
    Editor& editor_tl = *ed_tl_res.value();

    ShiftTrackClips cmd_target_locked{fix_target_locked.seq_id, fix_target_locked.v_track_id,
                                      time_f(100), dur_f(30)};
    auto res_tl = editor_tl.execute(cmd_target_locked);
    EXPECT_TRUE(test::is_error(res_tl, core::ErrorCode::InvalidArgument));
    EXPECT_NE(res_tl.error().message().find("locked"), std::string_view::npos);

    // 2. Locked partner track (project unchanged)
    auto fix_partner_locked = create_shift_fixture(gen);
    fix_partner_locked.project.sequences.at(fix_partner_locked.seq_id).tracks[1].locked = true;
    auto ed_pl_res = Editor::create(std::move(fix_partner_locked.project), gen);
    ASSERT_TRUE(test::is_ok(ed_pl_res));
    Editor& editor_pl = *ed_pl_res.value();
    const auto before_pl = editor_pl.snapshot();

    ShiftTrackClips cmd_partner_locked{fix_partner_locked.seq_id, fix_partner_locked.v_track_id,
                                       time_f(100), dur_f(30)};
    auto res_pl = editor_pl.execute(cmd_partner_locked);
    EXPECT_TRUE(test::is_error(res_pl, core::ErrorCode::InvalidArgument));
    EXPECT_NE(res_pl.error().message().find("locked"), std::string_view::npos);
    EXPECT_TRUE(model::identical(*editor_pl.snapshot(), *before_pl));

    // 3. Unknown track: NotFound
    const auto unknown_track_id = model::generate_id<model::TrackId>(gen);
    ShiftTrackClips cmd_unk_trk{fix.seq_id, unknown_track_id, time_f(100), dur_f(30)};
    auto res_unk_trk = editor.execute(cmd_unk_trk);
    EXPECT_TRUE(test::is_error(res_unk_trk, core::ErrorCode::NotFound));

    // 4. Unknown sequence: NotFound
    const auto unknown_seq_id = model::generate_id<model::SequenceId>(gen);
    ShiftTrackClips cmd_unk_seq{unknown_seq_id, fix.v_track_id, time_f(100), dur_f(30)};
    auto res_unk_seq = editor.execute(cmd_unk_seq);
    EXPECT_TRUE(test::is_error(res_unk_seq, core::ErrorCode::NotFound));

    // 5. snap(at) < 0: InvalidArgument
    ShiftTrackClips cmd_neg_at{fix.seq_id, fix.v_track_id, time_f(-1), dur_f(30)};
    auto res_neg_at = editor.execute(cmd_neg_at);
    EXPECT_TRUE(test::is_error(res_neg_at, core::ErrorCode::InvalidArgument));
}

// H12: unlinked clips on other tracks are never touched. created_clips is empty.
TEST(ShiftTrackClipsTest, H12_UnlinkedClipsOnOtherTracksUntouched) {
    core::UuidGenerator gen(9012ULL);
    auto fix = create_shift_fixture(gen);

    // Add unlinked audio clip D [100, 50] on track 1 (Audio)
    const auto clip_d_id = model::generate_id<model::ClipId>(gen);
    model::Clip d;
    d.id = clip_d_id;
    d.name = "ClipD";
    d.start = time_f(100);
    d.duration = dur_f(50);
    d.source_in = src_f(0);
    d.speed = model::Speed::normal();
    d.content = model::AudioContent{fix.video_media_id};
    // Make sure it doesn't overlap B' on track 1: move D to 200F
    d.start = time_f(200);
    fix.project.sequences.at(fix.seq_id).tracks[1].clips.push_back(d);

    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    ShiftTrackClips cmd{fix.seq_id, fix.v_track_id, time_f(100), dur_f(30)};
    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));
    EXPECT_TRUE(rt_res.value().created_clips.empty());

    const auto& a_clips = editor.snapshot()->sequences.at(fix.seq_id).tracks[1].clips;
    EXPECT_EQ(a_clips[0].start, time_f(130));  // B' linked moved to 130
    EXPECT_EQ(a_clips[1].start, time_f(200));  // D unlinked untouched at 200
}

}  // namespace
}  // namespace nxtcut::commands
