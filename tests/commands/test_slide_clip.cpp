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

struct SlideFixture {
    model::Project project;
    model::SequenceId seq_id;
    model::ClipId clip_l_id;
    model::ClipId clip_c_id;
    model::ClipId clip_r_id;
    model::MediaId video_media_id;
};

SlideFixture create_slide_fixture(core::UuidGenerator& gen, std::int64_t media_dur_f = 1000) {
    auto project = model::test::build_valid_project(gen);
    const auto seq_id = project.main_sequence;
    auto& seq = project.sequences.at(seq_id);
    seq.frame_rate = core::frame_rates::k24;

    const auto video_media_id = std::get<model::VideoContent>(seq.tracks[0].clips[0].content).media;
    project.media[video_media_id].duration = dur_f(media_dur_f);
    project.media[video_media_id].video->frame_rate = core::frame_rates::k24;

    const auto clip_l_id = model::generate_id<model::ClipId>(gen);
    const auto clip_c_id = model::generate_id<model::ClipId>(gen);
    const auto clip_r_id = model::generate_id<model::ClipId>(gen);

    // L [0, 100] src_in 100
    model::Clip l;
    l.id = clip_l_id;
    l.name = "ClipL";
    l.start = time_f(0);
    l.duration = dur_f(100);
    l.source_in = src_f(100);
    l.speed = model::Speed::normal();
    l.content = model::VideoContent{video_media_id};

    // C [100, 50] src_in 0
    model::Clip c;
    c.id = clip_c_id;
    c.name = "ClipC";
    c.start = time_f(100);
    c.duration = dur_f(50);
    c.source_in = src_f(0);
    c.speed = model::Speed::normal();
    c.content = model::VideoContent{video_media_id};

    // R [150, 100] src_in 100
    model::Clip r;
    r.id = clip_r_id;
    r.name = "ClipR";
    r.start = time_f(150);
    r.duration = dur_f(100);
    r.source_in = src_f(100);
    r.speed = model::Speed::normal();
    r.content = model::VideoContent{video_media_id};

    seq.tracks[0].clips.clear();
    seq.tracks[0].clips.push_back(l);
    seq.tracks[0].clips.push_back(c);
    seq.tracks[0].clips.push_back(r);

    seq.tracks[1].clips.clear();

    return SlideFixture{std::move(project), seq_id,    clip_l_id,
                        clip_c_id,          clip_r_id, video_media_id};
}

TEST(SlideClipTest, D1_SlidePositive20F) {
    core::UuidGenerator gen(10001ULL);
    auto fix = create_slide_fixture(gen);
    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    SlideClip cmd{fix.seq_id, fix.clip_c_id, time_f(120)};
    ASSERT_TRUE(test::is_ok(test::round_trip(editor, cmd)));

    const auto& clips = editor.snapshot()->sequences.at(fix.seq_id).tracks[0].clips;

    // L dur 120 (src_in 100)
    EXPECT_EQ(clips[0].start, time_f(0));
    EXPECT_EQ(clips[0].duration, dur_f(120));
    EXPECT_EQ(clips[0].source_in, src_f(100));

    // C start 120 (dur 50, src_in 0)
    EXPECT_EQ(clips[1].start, time_f(120));
    EXPECT_EQ(clips[1].duration, dur_f(50));
    EXPECT_EQ(clips[1].source_in, src_f(0));

    // R start 170, dur 80, src_in 120
    EXPECT_EQ(clips[2].start, time_f(170));
    EXPECT_EQ(clips[2].duration, dur_f(80));
    EXPECT_EQ(clips[2].source_in, src_f(120));
}

TEST(SlideClipTest, D2_SlideNegative20F) {
    core::UuidGenerator gen(10002ULL);
    auto fix = create_slide_fixture(gen);
    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    SlideClip cmd{fix.seq_id, fix.clip_c_id, time_f(80)};
    ASSERT_TRUE(test::is_ok(test::round_trip(editor, cmd)));

    const auto& clips = editor.snapshot()->sequences.at(fix.seq_id).tracks[0].clips;

    // L dur 80
    EXPECT_EQ(clips[0].start, time_f(0));
    EXPECT_EQ(clips[0].duration, dur_f(80));

    // C start 80
    EXPECT_EQ(clips[1].start, time_f(80));
    EXPECT_EQ(clips[1].duration, dur_f(50));

    // R start 130, dur 120, src_in 80
    EXPECT_EQ(clips[2].start, time_f(130));
    EXPECT_EQ(clips[2].duration, dur_f(120));
    EXPECT_EQ(clips[2].source_in, src_f(80));
}

TEST(SlideClipTest, D3_LeftNeighborOneMinimaLimit) {
    core::UuidGenerator gen(10003ULL);
    auto fix = create_slide_fixture(gen);
    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    // new_start 1F (-99): L dur 1 (ok)
    SlideClip cmd_1f{fix.seq_id, fix.clip_c_id, time_f(1)};
    ASSERT_TRUE(test::is_ok(test::round_trip(editor, cmd_1f)));

    // new_start 0 (-100): InvalidArgument "one frame"
    SlideClip cmd_0{fix.seq_id, fix.clip_c_id, time_f(0)};
    auto res_0 = editor.execute(cmd_0);
    ASSERT_FALSE(res_0.has_value());
    EXPECT_EQ(res_0.error().code(), core::ErrorCode::InvalidArgument);
    EXPECT_NE(res_0.error().message().find("one frame"), std::string::npos);
}

TEST(SlideClipTest, D4_RightNeighborOneMinimaLimit) {
    core::UuidGenerator gen(10004ULL);
    auto fix = create_slide_fixture(gen);
    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    // new_start 199F (+99): R dur 1 (ok)
    SlideClip cmd_199f{fix.seq_id, fix.clip_c_id, time_f(199)};
    ASSERT_TRUE(test::is_ok(test::round_trip(editor, cmd_199f)));

    // new_start 200F (+100): InvalidArgument "one frame"
    SlideClip cmd_200f{fix.seq_id, fix.clip_c_id, time_f(200)};
    auto res_200 = editor.execute(cmd_200f);
    ASSERT_FALSE(res_200.has_value());
    EXPECT_EQ(res_200.error().code(), core::ErrorCode::InvalidArgument);
    EXPECT_NE(res_200.error().message().find("one frame"), std::string::npos);
}

TEST(SlideClipTest, D5_SubframeSnapping) {
    core::UuidGenerator gen(10005ULL);
    auto fix = create_slide_fixture(gen);
    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    // new_start 100F -> empty
    SlideClip cmd_100{fix.seq_id, fix.clip_c_id, time_f(100)};
    auto cs_100 = cmd_100.build(*editor.snapshot(), gen);
    ASSERT_TRUE(test::is_ok(cs_100));
    EXPECT_TRUE(cs_100.value().empty());

    // 100F + 14,700,000 ticks acts as 101F
    SlideClip cmd_half{fix.seq_id, fix.clip_c_id,
                       model::TimelineTime::from_ticks(time_f(100).ticks() + 14'700'000)};
    ASSERT_TRUE(test::is_ok(test::round_trip(editor, cmd_half)));

    const auto& clips = editor.snapshot()->sequences.at(fix.seq_id).tracks[0].clips;
    EXPECT_EQ(clips[0].duration, dur_f(101));
    EXPECT_EQ(clips[1].start, time_f(101));
    EXPECT_EQ(clips[2].start, time_f(151));
    EXPECT_EQ(clips[2].duration, dur_f(99));
}

TEST(SlideClipTest, D6_MissingNeighborsValidation) {
    // C at 0 (no left neighbor)
    {
        core::UuidGenerator gen(10006ULL);
        auto fix = create_slide_fixture(gen);
        // Remove L, move C to 0
        fix.project.sequences.at(fix.seq_id)
            .tracks[0]
            .clips.erase(fix.project.sequences.at(fix.seq_id).tracks[0].clips.begin());
        fix.project.sequences.at(fix.seq_id).tracks[0].clips[0].start = time_f(0);
        fix.project.sequences.at(fix.seq_id).tracks[0].clips[1].start = time_f(50);

        auto ed_res = Editor::create(std::move(fix.project), gen);
        ASSERT_TRUE(test::is_ok(ed_res));
        Editor& editor = *ed_res.value();

        SlideClip cmd{fix.seq_id, fix.clip_c_id, time_f(10)};
        auto res = editor.execute(cmd);
        ASSERT_FALSE(res.has_value());
        EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
        EXPECT_NE(res.error().message().find("neighbor"), std::string::npos);
    }

    // Gap on left side (L at 0..90, gap 90..100, C at 100..150)
    {
        core::UuidGenerator gen2(10007ULL);
        auto fix2 = create_slide_fixture(gen2);
        fix2.project.sequences.at(fix2.seq_id).tracks[0].clips[0].duration = dur_f(90);

        auto ed_res2 = Editor::create(std::move(fix2.project), gen2);
        ASSERT_TRUE(test::is_ok(ed_res2));
        Editor& ed2 = *ed_res2.value();

        SlideClip cmd{fix2.seq_id, fix2.clip_c_id, time_f(110)};
        auto res = ed2.execute(cmd);
        ASSERT_FALSE(res.has_value());
        EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
        EXPECT_NE(res.error().message().find("neighbor"), std::string::npos);
    }

    // Gap on right side (R at 160..260)
    {
        core::UuidGenerator gen3(10008ULL);
        auto fix3 = create_slide_fixture(gen3);
        fix3.project.sequences.at(fix3.seq_id).tracks[0].clips[2].start = time_f(160);

        auto ed_res3 = Editor::create(std::move(fix3.project), gen3);
        ASSERT_TRUE(test::is_ok(ed_res3));
        Editor& ed3 = *ed_res3.value();

        SlideClip cmd{fix3.seq_id, fix3.clip_c_id, time_f(110)};
        auto res = ed3.execute(cmd);
        ASSERT_FALSE(res.has_value());
        EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
        EXPECT_NE(res.error().message().find("neighbor"), std::string::npos);
    }
}

TEST(SlideClipTest, D7_SourceBoundsAndMediaDurationLimits) {
    // R src_in 5F and slide -10F: InvalidArgument (source before zero)
    {
        core::UuidGenerator gen(10009ULL);
        auto fix = create_slide_fixture(gen);
        fix.project.sequences.at(fix.seq_id).tracks[0].clips[2].source_in = src_f(5);

        auto ed_res = Editor::create(std::move(fix.project), gen);
        ASSERT_TRUE(test::is_ok(ed_res));
        Editor& editor = *ed_res.value();

        SlideClip cmd{fix.seq_id, fix.clip_c_id, time_f(90)};  // -10F slide
        auto res = editor.execute(cmd);
        ASSERT_FALSE(res.has_value());
        EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
    }

    // Media duration 250F, L src_in 139F (L uses source 139F..239F): sliding +12F would extend L's
    // tail to source 251F > 250F: InvalidArgument (media duration)
    {
        core::UuidGenerator gen2(10010ULL);
        auto fix2 = create_slide_fixture(gen2, 250);
        fix2.project.sequences.at(fix2.seq_id).tracks[0].clips[0].source_in = src_f(139);

        auto ed_res2 = Editor::create(std::move(fix2.project), gen2);
        ASSERT_TRUE(test::is_ok(ed_res2));
        Editor& ed2 = *ed_res2.value();

        SlideClip cmd2{fix2.seq_id, fix2.clip_c_id, time_f(112)};  // +12F slide
        auto res2 = ed2.execute(cmd2);
        ASSERT_FALSE(res2.has_value());
        EXPECT_EQ(res2.error().code(), core::ErrorCode::InvalidArgument);

        // +11F still fits exactly (139F + 111F = 250F)
        SlideClip cmd_ok{fix2.seq_id, fix2.clip_c_id, time_f(111)};
        ASSERT_TRUE(test::is_ok(test::round_trip(ed2, cmd_ok)));
    }
}

TEST(SlideClipTest, D8_LinkedAudioVideoTrios) {
    core::UuidGenerator gen(10011ULL);
    auto fix = create_slide_fixture(gen);
    auto& seq = fix.project.sequences.at(fix.seq_id);

    const auto link_c = model::generate_id<model::LinkId>(gen);
    const auto l_prime_id = model::generate_id<model::ClipId>(gen);
    const auto c_prime_id = model::generate_id<model::ClipId>(gen);
    const auto r_prime_id = model::generate_id<model::ClipId>(gen);

    seq.tracks[0].clips[1].link_id = link_c;

    model::Clip l_prime;
    l_prime.id = l_prime_id;
    l_prime.start = time_f(0);
    l_prime.duration = dur_f(100);
    l_prime.source_in = src_f(100);
    l_prime.content = model::AudioContent{fix.video_media_id};

    model::Clip c_prime;
    c_prime.id = c_prime_id;
    c_prime.start = time_f(100);
    c_prime.duration = dur_f(50);
    c_prime.source_in = src_f(0);
    c_prime.link_id = link_c;
    c_prime.content = model::AudioContent{fix.video_media_id};

    model::Clip r_prime;
    r_prime.id = r_prime_id;
    r_prime.start = time_f(150);
    r_prime.duration = dur_f(100);
    r_prime.source_in = src_f(100);
    r_prime.content = model::AudioContent{fix.video_media_id};

    seq.tracks[1].clips.push_back(l_prime);
    seq.tracks[1].clips.push_back(c_prime);
    seq.tracks[1].clips.push_back(r_prime);

    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    // Slide C by +20F changes all six clips like D1
    SlideClip cmd{fix.seq_id, fix.clip_c_id, time_f(120)};
    ASSERT_TRUE(test::is_ok(test::round_trip(editor, cmd)));

    const auto& v_clips = editor.snapshot()->sequences.at(fix.seq_id).tracks[0].clips;
    const auto& a_clips = editor.snapshot()->sequences.at(fix.seq_id).tracks[1].clips;

    EXPECT_EQ(v_clips[0].duration, dur_f(120));
    EXPECT_EQ(v_clips[1].start, time_f(120));
    EXPECT_EQ(v_clips[2].start, time_f(170));
    EXPECT_EQ(v_clips[2].duration, dur_f(80));

    EXPECT_EQ(a_clips[0].duration, dur_f(120));
    EXPECT_EQ(a_clips[1].start, time_f(120));
    EXPECT_EQ(a_clips[2].start, time_f(170));
    EXPECT_EQ(a_clips[2].duration, dur_f(80));

    // ignore_links changes only V1 trio
    SlideClip cmd_ign{fix.seq_id, fix.clip_c_id, time_f(110), true};
    ASSERT_TRUE(test::is_ok(test::round_trip(editor, cmd_ign)));

    const auto& v2 = editor.snapshot()->sequences.at(fix.seq_id).tracks[0].clips;
    const auto& a2 = editor.snapshot()->sequences.at(fix.seq_id).tracks[1].clips;

    EXPECT_EQ(v2[1].start, time_f(110));
    EXPECT_EQ(a2[1].start, time_f(120));  // Audio untouched

    // If C' has a gap on one side: InvalidArgument "neighbor", project unchanged
    {
        core::UuidGenerator gen_gap(10012ULL);
        auto fix_gap = create_slide_fixture(gen_gap);
        auto& seq_gap = fix_gap.project.sequences.at(fix_gap.seq_id);
        const auto link_c_gap = model::generate_id<model::LinkId>(gen_gap);
        seq_gap.tracks[0].clips[1].link_id = link_c_gap;

        // The audio copies must reference THIS fixture's media, not the outer project's.
        const model::ClipContent gap_audio = model::AudioContent{fix_gap.video_media_id};

        model::Clip c_prime_gap = c_prime;
        c_prime_gap.link_id = link_c_gap;
        c_prime_gap.content = gap_audio;
        model::Clip l_prime_gap = l_prime;
        l_prime_gap.content = gap_audio;
        seq_gap.tracks[1].clips.push_back(l_prime_gap);
        seq_gap.tracks[1].clips.push_back(c_prime_gap);
        // Gap before R': starts at 160F instead of 150F
        model::Clip r_prime_gap = r_prime;
        r_prime_gap.content = gap_audio;
        r_prime_gap.start = time_f(160);
        seq_gap.tracks[1].clips.push_back(r_prime_gap);

        auto ed_gap_res = Editor::create(std::move(fix_gap.project), gen_gap);
        ASSERT_TRUE(test::is_ok(ed_gap_res));
        Editor& ed_gap = *ed_gap_res.value();

        const auto before_gap = ed_gap.snapshot();
        SlideClip cmd_gap{fix_gap.seq_id, fix_gap.clip_c_id, time_f(120), false};
        auto res_gap = ed_gap.execute(cmd_gap);
        ASSERT_FALSE(res_gap.has_value());
        EXPECT_EQ(res_gap.error().code(), core::ErrorCode::InvalidArgument);
        EXPECT_NE(res_gap.error().message().find("neighbor"), std::string::npos);
        EXPECT_TRUE(model::identical(*ed_gap.snapshot(), *before_gap));
    }
}

TEST(SlideClipTest, D9_AmbiguousRolesFails) {
    core::UuidGenerator gen(10013ULL);
    auto fix = create_slide_fixture(gen);
    auto& seq = fix.project.sequences.at(fix.seq_id);

    // Two touching clips X, Y linked to each other on one track, both would slide
    // Link L and C together
    const auto link_lc = model::generate_id<model::LinkId>(gen);
    seq.tracks[0].clips[0].link_id = link_lc;
    seq.tracks[0].clips[1].link_id = link_lc;

    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    SlideClip cmd{fix.seq_id, fix.clip_c_id, time_f(120), false};
    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
    EXPECT_NE(res.error().message().find("ambiguous"), std::string::npos);
}

TEST(SlideClipTest, D10_LockedTrackFailsWithLockedMessage) {
    core::UuidGenerator gen(10014ULL);
    auto fix = create_slide_fixture(gen);
    fix.project.sequences.at(fix.seq_id).tracks[0].locked = true;

    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    SlideClip cmd{fix.seq_id, fix.clip_c_id, time_f(120)};
    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
    EXPECT_NE(res.error().message().find("locked"), std::string::npos);
}

TEST(SlideClipTest, D11_SpeedOneEleventhExactRounding) {
    core::UuidGenerator gen(10015ULL);
    auto fix = create_slide_fixture(gen);
    auto& seq = fix.project.sequences.at(fix.seq_id);

    // speed 1/11 on R
    seq.tracks[0].clips[2].speed = model::Speed::create(1, 11).value();

    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    // slide +1F (new_start 101F) raises R.source_in by exactly 2,672,727 ticks
    SlideClip cmd{fix.seq_id, fix.clip_c_id, time_f(101)};
    ASSERT_TRUE(test::is_ok(test::round_trip(editor, cmd)));

    const auto& r = editor.snapshot()->sequences.at(fix.seq_id).tracks[0].clips[2];
    EXPECT_EQ(r.source_in.ticks(), src_f(100).ticks() + 2'672'727);
}

TEST(SlideClipTest, D12_ClipsElsewhereUntouchedAndEmptyCreatedClips) {
    core::UuidGenerator gen(10016ULL);
    auto fix = create_slide_fixture(gen);
    auto& seq = fix.project.sequences.at(fix.seq_id);

    // Clip elsewhere on Track 0 at 300..350F
    const auto clip_other_id = model::generate_id<model::ClipId>(gen);
    model::Clip other;
    other.id = clip_other_id;
    other.name = "Other";
    other.start = time_f(300);
    other.duration = dur_f(50);
    other.source_in = src_f(0);
    other.content = model::VideoContent{fix.video_media_id};
    seq.tracks[0].clips.push_back(other);

    auto ed_res = Editor::create(std::move(fix.project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    SlideClip cmd{fix.seq_id, fix.clip_c_id, time_f(120)};
    auto receipt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(receipt_res));
    EXPECT_TRUE(receipt_res.value().created_clips.empty());

    const auto& clips = editor.snapshot()->sequences.at(fix.seq_id).tracks[0].clips;
    EXPECT_EQ(clips[3].id, clip_other_id);
    EXPECT_EQ(clips[3].start, time_f(300));
    EXPECT_EQ(clips[3].duration, dur_f(50));
}

}  // namespace
}  // namespace nxtcut::commands
