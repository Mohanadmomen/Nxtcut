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

struct RollFixture {
    model::Project project;
    model::SequenceId seq_id;
    model::ClipId clip_a_id;
    model::ClipId clip_b_id;
    model::MediaId video_media_id;
};

RollFixture create_roll_fixture(core::UuidGenerator& gen, std::int64_t media_dur_f = 1000) {
    auto project = model::test::build_valid_project(gen);
    const auto seq_id = project.main_sequence;
    auto& seq = project.sequences.at(seq_id);
    seq.frame_rate = core::frame_rates::k24;

    const auto video_media_id = std::get<model::VideoContent>(seq.tracks[0].clips[0].content).media;
    project.media[video_media_id].duration = dur_f(media_dur_f);
    project.media[video_media_id].video->frame_rate = core::frame_rates::k24;

    const auto clip_a_id = model::generate_id<model::ClipId>(gen);
    const auto clip_b_id = model::generate_id<model::ClipId>(gen);

    model::Clip a;
    a.id = clip_a_id;
    a.name = "ClipA";
    a.start = time_f(0);
    a.duration = dur_f(100);
    a.source_in = src_f(0);
    a.speed = model::Speed::normal();
    a.content = model::VideoContent{video_media_id};

    model::Clip b;
    b.id = clip_b_id;
    b.name = "ClipB";
    b.start = time_f(100);
    b.duration = dur_f(100);
    b.source_in = src_f(200);
    b.speed = model::Speed::normal();
    b.content = model::VideoContent{video_media_id};

    seq.tracks[0].clips.clear();
    seq.tracks[0].clips.push_back(a);
    seq.tracks[0].clips.push_back(b);

    seq.tracks[1].clips.clear();

    return RollFixture{std::move(project), seq_id, clip_a_id, clip_b_id, video_media_id};
}

TEST(RollEditTest, R1_RollTo90FShortensLeftAndExtendsRight) {
    core::UuidGenerator gen(8001ULL);
    auto fix = create_roll_fixture(gen);
    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    RollEdit cmd{fix.seq_id, fix.clip_a_id, fix.clip_b_id, time_f(90)};
    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& clips = editor.snapshot()->sequences.at(fix.seq_id).tracks[0].clips;
    EXPECT_EQ(clips[0].start, time_f(0));
    EXPECT_EQ(clips[0].duration, dur_f(90));
    EXPECT_EQ(clips[0].source_in, src_f(0));

    EXPECT_EQ(clips[1].start, time_f(90));
    EXPECT_EQ(clips[1].duration, dur_f(110));
    EXPECT_EQ(clips[1].source_in, src_f(190));
}

TEST(RollEditTest, R2_RollTo120FExtendsLeftAndShortensRight) {
    core::UuidGenerator gen(8002ULL);
    auto fix = create_roll_fixture(gen);
    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    RollEdit cmd{fix.seq_id, fix.clip_a_id, fix.clip_b_id, time_f(120)};
    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& clips = editor.snapshot()->sequences.at(fix.seq_id).tracks[0].clips;
    EXPECT_EQ(clips[0].start, time_f(0));
    EXPECT_EQ(clips[0].duration, dur_f(120));
    EXPECT_EQ(clips[0].source_in, src_f(0));

    EXPECT_EQ(clips[1].start, time_f(120));
    EXPECT_EQ(clips[1].duration, dur_f(80));
    EXPECT_EQ(clips[1].source_in, src_f(220));
}

TEST(RollEditTest, R3_SubframeSnapping) {
    core::UuidGenerator gen(8003ULL);
    auto fix = create_roll_fixture(gen);
    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    // 100F: exact no-op -> empty ChangeSet
    RollEdit cmd_exact{fix.seq_id, fix.clip_a_id, fix.clip_b_id, time_f(100)};
    auto cs_exact = cmd_exact.build(*editor.snapshot(), gen);
    ASSERT_TRUE(test::is_ok(cs_exact));
    EXPECT_TRUE(cs_exact.value().empty());

    // 100F + 14,699,999 ticks: snaps to 100F -> empty ChangeSet
    RollEdit cmd_sub{fix.seq_id, fix.clip_a_id, fix.clip_b_id,
                     model::TimelineTime::from_ticks(time_f(100).ticks() + 14'699'999)};
    auto cs_sub = cmd_sub.build(*editor.snapshot(), gen);
    ASSERT_TRUE(test::is_ok(cs_sub));
    EXPECT_TRUE(cs_sub.value().empty());

    // 100F + 14,700,000 ticks: snaps to 101F -> behaves like 101F
    RollEdit cmd_half{fix.seq_id, fix.clip_a_id, fix.clip_b_id,
                      model::TimelineTime::from_ticks(time_f(100).ticks() + 14'700'000)};
    auto rt_res = test::round_trip(editor, cmd_half);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& clips = editor.snapshot()->sequences.at(fix.seq_id).tracks[0].clips;
    EXPECT_EQ(clips[0].duration, dur_f(101));
    EXPECT_EQ(clips[1].start, time_f(101));
    EXPECT_EQ(clips[1].duration, dur_f(99));
    EXPECT_EQ(clips[1].source_in, src_f(201));
}

TEST(RollEditTest, R4_OneMinimaDurationLimit) {
    core::UuidGenerator gen(8004ULL);
    auto fix = create_roll_fixture(gen);
    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    // new_edit 1F: A dur 1 (ok)
    RollEdit cmd_1f{fix.seq_id, fix.clip_a_id, fix.clip_b_id, time_f(1)};
    ASSERT_TRUE(test::is_ok(test::round_trip(editor, cmd_1f)));

    // new_edit 0: InvalidArgument "one frame"
    RollEdit cmd_0{fix.seq_id, fix.clip_a_id, fix.clip_b_id, time_f(0)};
    auto res_0 = editor.execute(cmd_0);
    ASSERT_FALSE(res_0.has_value());
    EXPECT_EQ(res_0.error().code(), core::ErrorCode::InvalidArgument);
    EXPECT_NE(res_0.error().message().find("one frame"), std::string::npos);

    // new_edit 199F: B dur 1 (ok)
    RollEdit cmd_199f{fix.seq_id, fix.clip_a_id, fix.clip_b_id, time_f(199)};
    ASSERT_TRUE(test::is_ok(test::round_trip(editor, cmd_199f)));

    // new_edit 200F: InvalidArgument "one frame"
    RollEdit cmd_200f{fix.seq_id, fix.clip_a_id, fix.clip_b_id, time_f(200)};
    auto res_200 = editor.execute(cmd_200f);
    ASSERT_FALSE(res_200.has_value());
    EXPECT_EQ(res_200.error().code(), core::ErrorCode::InvalidArgument);
    EXPECT_NE(res_200.error().message().find("one frame"), std::string::npos);
}

TEST(RollEditTest, R5_SourceBeforeZeroFailsAndSnapshotUnchanged) {
    core::UuidGenerator gen(8005ULL);
    auto fix = create_roll_fixture(gen);

    // Set B src_in = 5F
    fix.project.sequences.at(fix.seq_id).tracks[0].clips[1].source_in = src_f(5);

    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto before = editor.snapshot();

    // Roll to 90F (-10F): B source_in would become 5F - 10F < 0
    RollEdit cmd{fix.seq_id, fix.clip_a_id, fix.clip_b_id, time_f(90)};
    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);

    // Snapshot unchanged
    EXPECT_TRUE(model::identical(*editor.snapshot(), *before));
}

TEST(RollEditTest, R6_MediaDurationHandleLimit) {
    core::UuidGenerator gen(8006ULL);
    // Media duration 110F, A src_in 0
    auto fix = create_roll_fixture(gen, 110);
    // B must fit inside the 110F media too (the default src_in 200F would not).
    fix.project.sequences.at(fix.seq_id).tracks[0].clips[1].source_in = src_f(0);
    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    // new_edit 110F ok
    RollEdit cmd_110{fix.seq_id, fix.clip_a_id, fix.clip_b_id, time_f(110)};
    ASSERT_TRUE(test::is_ok(test::round_trip(editor, cmd_110)));

    // new_edit 111F: InvalidArgument (media duration)
    RollEdit cmd_111{fix.seq_id, fix.clip_a_id, fix.clip_b_id, time_f(111)};
    auto res_111 = editor.execute(cmd_111);
    ASSERT_FALSE(res_111.has_value());
    EXPECT_EQ(res_111.error().code(), core::ErrorCode::InvalidArgument);
}

TEST(RollEditTest, R7_AdjacencyAndExistenceValidation) {
    core::UuidGenerator gen(8007ULL);
    auto fix = create_roll_fixture(gen);
    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    // left == right -> InvalidArgument "adjacent"
    RollEdit cmd_same{fix.seq_id, fix.clip_a_id, fix.clip_a_id, time_f(90)};
    auto res_same = editor.execute(cmd_same);
    ASSERT_FALSE(res_same.has_value());
    EXPECT_EQ(res_same.error().code(), core::ErrorCode::InvalidArgument);
    EXPECT_NE(res_same.error().message().find("adjacent"), std::string::npos);

    // left and right swapped (B, A) -> InvalidArgument "adjacent"
    RollEdit cmd_swapped{fix.seq_id, fix.clip_b_id, fix.clip_a_id, time_f(90)};
    auto res_swapped = editor.execute(cmd_swapped);
    ASSERT_FALSE(res_swapped.has_value());
    EXPECT_EQ(res_swapped.error().code(), core::ErrorCode::InvalidArgument);
    EXPECT_NE(res_swapped.error().message().find("adjacent"), std::string::npos);

    // Unknown clip -> NotFound
    const auto unknown_id = model::generate_id<model::ClipId>(gen);
    RollEdit cmd_unknown{fix.seq_id, unknown_id, fix.clip_b_id, time_f(90)};
    auto res_unknown = editor.execute(cmd_unknown);
    ASSERT_FALSE(res_unknown.has_value());
    EXPECT_EQ(res_unknown.error().code(), core::ErrorCode::NotFound);

    // Gap between A and B
    {
        core::UuidGenerator gen2(8008ULL);
        auto fix_gap = create_roll_fixture(gen2);
        fix_gap.project.sequences.at(fix_gap.seq_id).tracks[0].clips[1].start = time_f(101);
        auto ed_gap_res = Editor::create(std::move(fix_gap.project), gen2);
        ASSERT_TRUE(test::is_ok(ed_gap_res));
        Editor& ed_gap = *ed_gap_res.value();

        RollEdit cmd_gap{fix_gap.seq_id, fix_gap.clip_a_id, fix_gap.clip_b_id, time_f(90)};
        auto res_gap = ed_gap.execute(cmd_gap);
        ASSERT_FALSE(res_gap.has_value());
        EXPECT_EQ(res_gap.error().code(), core::ErrorCode::InvalidArgument);
        EXPECT_NE(res_gap.error().message().find("adjacent"), std::string::npos);
    }

    // Clips on different tracks
    {
        core::UuidGenerator gen3(8009ULL);
        auto fix_diff = create_roll_fixture(gen3);
        const auto b_clip = fix_diff.project.sequences.at(fix_diff.seq_id).tracks[0].clips[1];
        fix_diff.project.sequences.at(fix_diff.seq_id).tracks[0].clips.pop_back();

        // Put B on audio track (with AudioContent)
        model::Clip b_audio = b_clip;
        b_audio.content = model::AudioContent{fix_diff.video_media_id};
        fix_diff.project.sequences.at(fix_diff.seq_id).tracks[1].clips.push_back(b_audio);

        auto ed_diff_res = Editor::create(std::move(fix_diff.project), gen3);
        ASSERT_TRUE(test::is_ok(ed_diff_res));
        Editor& ed_diff = *ed_diff_res.value();

        RollEdit cmd_diff{fix_diff.seq_id, fix_diff.clip_a_id, fix_diff.clip_b_id, time_f(90)};
        auto res_diff = ed_diff.execute(cmd_diff);
        ASSERT_FALSE(res_diff.has_value());
        EXPECT_EQ(res_diff.error().code(), core::ErrorCode::InvalidArgument);
        EXPECT_NE(res_diff.error().message().find("adjacent"), std::string::npos);
    }
}

TEST(RollEditTest, R8_LockedTrackFailsWithLockedMessage) {
    core::UuidGenerator gen(8010ULL);
    auto fix = create_roll_fixture(gen);
    fix.project.sequences.at(fix.seq_id).tracks[0].locked = true;

    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    RollEdit cmd{fix.seq_id, fix.clip_a_id, fix.clip_b_id, time_f(90)};
    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
    EXPECT_NE(res.error().message().find("locked"), std::string::npos);
}

TEST(RollEditTest, R9_LinkedAudioVideoPartners) {
    core::UuidGenerator gen(8011ULL);
    auto fix = create_roll_fixture(gen);
    auto& seq = fix.project.sequences.at(fix.seq_id);

    const auto link_a = model::generate_id<model::LinkId>(gen);
    const auto link_b = model::generate_id<model::LinkId>(gen);
    const auto a_prime_id = model::generate_id<model::ClipId>(gen);
    const auto b_prime_id = model::generate_id<model::ClipId>(gen);

    seq.tracks[0].clips[0].link_id = link_a;
    seq.tracks[0].clips[1].link_id = link_b;

    model::Clip a_prime;
    a_prime.id = a_prime_id;
    a_prime.name = "A_Audio";
    a_prime.start = time_f(0);
    a_prime.duration = dur_f(100);
    a_prime.source_in = src_f(0);
    a_prime.speed = model::Speed::normal();
    a_prime.link_id = link_a;
    a_prime.content = model::AudioContent{fix.video_media_id};

    model::Clip b_prime;
    b_prime.id = b_prime_id;
    b_prime.name = "B_Audio";
    b_prime.start = time_f(100);
    b_prime.duration = dur_f(100);
    b_prime.source_in = src_f(200);
    b_prime.speed = model::Speed::normal();
    b_prime.link_id = link_b;
    b_prime.content = model::AudioContent{fix.video_media_id};

    seq.tracks[1].clips.push_back(a_prime);
    seq.tracks[1].clips.push_back(b_prime);

    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    // Roll to 90F: all 4 change
    RollEdit cmd{fix.seq_id, fix.clip_a_id, fix.clip_b_id, time_f(90)};
    ASSERT_TRUE(test::is_ok(test::round_trip(editor, cmd)));

    const auto& v_clips = editor.snapshot()->sequences.at(fix.seq_id).tracks[0].clips;
    const auto& a_clips = editor.snapshot()->sequences.at(fix.seq_id).tracks[1].clips;

    EXPECT_EQ(v_clips[0].duration, dur_f(90));
    EXPECT_EQ(v_clips[1].start, time_f(90));
    EXPECT_EQ(v_clips[1].duration, dur_f(110));
    EXPECT_EQ(v_clips[1].source_in, src_f(190));

    EXPECT_EQ(a_clips[0].duration, dur_f(90));
    EXPECT_EQ(a_clips[1].start, time_f(90));
    EXPECT_EQ(a_clips[1].duration, dur_f(110));
    EXPECT_EQ(a_clips[1].source_in, src_f(190));

    // ignore_links changes only video
    RollEdit cmd_ignore{fix.seq_id, fix.clip_a_id, fix.clip_b_id, time_f(80), true};
    ASSERT_TRUE(test::is_ok(test::round_trip(editor, cmd_ignore)));

    const auto& v2 = editor.snapshot()->sequences.at(fix.seq_id).tracks[0].clips;
    const auto& a2 = editor.snapshot()->sequences.at(fix.seq_id).tracks[1].clips;
    EXPECT_EQ(v2[0].duration, dur_f(80));
    EXPECT_EQ(a2[0].duration, dur_f(90));  // Audio left untouched

    // If A and B share one LinkId: InvalidArgument "same link"
    {
        core::UuidGenerator gen_same(8012ULL);
        auto fix_same = create_roll_fixture(gen_same);
        const auto shared_link = model::generate_id<model::LinkId>(gen_same);
        fix_same.project.sequences.at(fix_same.seq_id).tracks[0].clips[0].link_id = shared_link;
        fix_same.project.sequences.at(fix_same.seq_id).tracks[0].clips[1].link_id = shared_link;

        auto ed_same_res = Editor::create(std::move(fix_same.project), gen_same);
        ASSERT_TRUE(test::is_ok(ed_same_res));
        Editor& ed_same = *ed_same_res.value();

        RollEdit cmd_sl{fix_same.seq_id, fix_same.clip_a_id, fix_same.clip_b_id, time_f(90), false};
        auto res_sl = ed_same.execute(cmd_sl);
        ASSERT_FALSE(res_sl.has_value());
        EXPECT_EQ(res_sl.error().code(), core::ErrorCode::InvalidArgument);
        EXPECT_NE(res_sl.error().message().find("same link"), std::string::npos);

        // with ignore_links = true, it succeeds
        RollEdit cmd_sl_ign{fix_same.seq_id, fix_same.clip_a_id, fix_same.clip_b_id, time_f(90),
                            true};
        ASSERT_TRUE(test::is_ok(test::round_trip(ed_same, cmd_sl_ign)));
    }
}

TEST(RollEditTest, R10_AudioFadesFitted) {
    core::UuidGenerator gen(8013ULL);
    auto fix = create_roll_fixture(gen);
    auto& seq = fix.project.sequences.at(fix.seq_id);

    const auto a_prime_id = model::generate_id<model::ClipId>(gen);
    const auto link_a = model::generate_id<model::LinkId>(gen);
    seq.tracks[0].clips[0].link_id = link_a;

    model::AudioContent ac{fix.video_media_id};
    ac.fade_in = dur_f(60);
    ac.fade_out = dur_f(40);

    model::Clip a_prime;
    a_prime.id = a_prime_id;
    a_prime.start = time_f(0);
    a_prime.duration = dur_f(100);
    a_prime.source_in = src_f(0);
    a_prime.speed = model::Speed::normal();
    a_prime.link_id = link_a;
    a_prime.content = ac;

    seq.tracks[1].clips.push_back(a_prime);

    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    // Roll to 90F gives A' dur 90, fade_in 60, fade_out 30
    RollEdit cmd{fix.seq_id, fix.clip_a_id, fix.clip_b_id, time_f(90)};
    ASSERT_TRUE(test::is_ok(test::round_trip(editor, cmd)));

    const auto& a_clips = editor.snapshot()->sequences.at(fix.seq_id).tracks[1].clips;
    EXPECT_EQ(a_clips[0].duration, dur_f(90));
    const auto& audio_content = std::get<model::AudioContent>(a_clips[0].content);
    EXPECT_EQ(audio_content.fade_in, dur_f(60));
    EXPECT_EQ(audio_content.fade_out, dur_f(30));
}

TEST(RollEditTest, R11_UntouchedClipsAndEmptyReceiptCreatedClips) {
    core::UuidGenerator gen(8014ULL);
    auto fix = create_roll_fixture(gen);
    auto& seq = fix.project.sequences.at(fix.seq_id);

    // Clip C after B at 200..250F
    const auto clip_c_id = model::generate_id<model::ClipId>(gen);
    model::Clip c;
    c.id = clip_c_id;
    c.name = "ClipC";
    c.start = time_f(200);
    c.duration = dur_f(50);
    c.source_in = src_f(0);
    c.content = model::VideoContent{fix.video_media_id};
    seq.tracks[0].clips.push_back(c);

    // Audio clip on track 1 at 300..350F
    const auto audio_other_id = model::generate_id<model::ClipId>(gen);
    model::Clip a_other;
    a_other.id = audio_other_id;
    a_other.name = "AudioOther";
    a_other.start = time_f(300);
    a_other.duration = dur_f(50);
    a_other.source_in = src_f(0);
    a_other.content = model::AudioContent{fix.video_media_id};
    seq.tracks[1].clips.push_back(a_other);

    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    RollEdit cmd{fix.seq_id, fix.clip_a_id, fix.clip_b_id, time_f(90)};
    auto receipt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(receipt_res));
    EXPECT_TRUE(receipt_res.value().created_clips.empty());

    const auto& v_clips = editor.snapshot()->sequences.at(fix.seq_id).tracks[0].clips;
    const auto& a_clips = editor.snapshot()->sequences.at(fix.seq_id).tracks[1].clips;

    // C untouched
    EXPECT_EQ(v_clips[2].id, clip_c_id);
    EXPECT_EQ(v_clips[2].start, time_f(200));
    EXPECT_EQ(v_clips[2].duration, dur_f(50));

    // A_other untouched
    EXPECT_EQ(a_clips[0].id, audio_other_id);
    EXPECT_EQ(a_clips[0].start, time_f(300));
    EXPECT_EQ(a_clips[0].duration, dur_f(50));
}

TEST(RollEditTest, R12_SpeedOneEleventhExactRoundingAndRestoration) {
    core::UuidGenerator gen(8015ULL);
    auto fix = create_roll_fixture(gen);
    auto& seq = fix.project.sequences.at(fix.seq_id);

    // Speed 1/11 on B
    seq.tracks[0].clips[1].speed = model::Speed::create(1, 11).value();

    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto initial_snapshot = editor.snapshot();

    // Roll by -1F (new_edit 99F)
    RollEdit cmd_minus1{fix.seq_id, fix.clip_a_id, fix.clip_b_id, time_f(99)};
    ASSERT_TRUE(test::is_ok(test::round_trip(editor, cmd_minus1)));

    const auto& b_clip = editor.snapshot()->sequences.at(fix.seq_id).tracks[0].clips[1];
    EXPECT_EQ(b_clip.source_in.ticks(), src_f(200).ticks() - 2'672'727);

    // Roll by +1F (new_edit 100F)
    RollEdit cmd_plus1{fix.seq_id, fix.clip_a_id, fix.clip_b_id, time_f(100)};
    ASSERT_TRUE(test::is_ok(test::round_trip(editor, cmd_plus1)));

    EXPECT_TRUE(model::identical(*editor.snapshot(), *initial_snapshot));
}

}  // namespace
}  // namespace nxtcut::commands
