#include <nxtcut/commands/editor.hpp>
#include <nxtcut/commands/timeline_commands.hpp>
#include <nxtcut/core/time.hpp>
#include <nxtcut/core/uuid.hpp>
#include <nxtcut/model/clip.hpp>
#include <nxtcut/model/ids.hpp>
#include <nxtcut/model/project.hpp>
#include <nxtcut/model/speed.hpp>
#include <nxtcut/model/time_coords.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "test_helper.hpp"
#include <nxtcut_test/assertions.hpp>
#include <nxtcut_test/model_fixtures.hpp>

namespace nxtcut::commands {
namespace {

using model::test::duration_of_seconds;
using model::test::timeline_at_seconds;

TEST(TrimClipTest, TailTrimV1from5to3sTrimsLinkedA1WithoutRipple) {
    core::UuidGenerator gen(7001ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto v1_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].id;

    // Tail trim V1 from 5s to 3s (without ripple)
    TrimClip cmd{main_seq_id, v1_id, TrimEdge::Tail, timeline_at_seconds(3), false};

    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& v_clips = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips;
    const auto& a_clips = editor.snapshot()->sequences.at(main_seq_id).tracks[1].clips;

    // V1 shortened to 3s, V2 stays at 5s
    EXPECT_EQ(v_clips[0].duration, duration_of_seconds(3));
    EXPECT_EQ(v_clips[1].start, timeline_at_seconds(5));

    // A1 (linked) trimmed to 3s too
    EXPECT_EQ(a_clips[0].duration, duration_of_seconds(3));
}

TEST(TrimClipTest, TailTrimV1from5to3sWithRippleShiftsLaterClips) {
    core::UuidGenerator gen(7002ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto v1_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].id;

    // Tail trim V1 from 5s to 3s with ripple
    TrimClip cmd{main_seq_id, v1_id, TrimEdge::Tail, timeline_at_seconds(3), true};

    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& v_clips = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips;
    // V1 is 0..3s
    EXPECT_EQ(v_clips[0].start, timeline_at_seconds(0));
    EXPECT_EQ(v_clips[0].duration, duration_of_seconds(3));
    // V2 is shifted by -2s: 3..8s
    EXPECT_EQ(v_clips[1].start, timeline_at_seconds(3));
    EXPECT_EQ(v_clips[1].duration, duration_of_seconds(5));
    // V3 is shifted by -2s: 8..13s
    EXPECT_EQ(v_clips[2].start, timeline_at_seconds(8));
    EXPECT_EQ(v_clips[2].duration, duration_of_seconds(5));
}

TEST(TrimClipTest, HeadTrimV2to7sWithoutRippleAdvancesSourceIn) {
    core::UuidGenerator gen(7003ULL);
    auto project = model::test::build_valid_project(gen);
    const auto main_seq_id = project.main_sequence;

    // Make V2 a Video clip referencing video.mp4 so we can test source_in advancement
    const auto video_media_id =
        std::get<model::VideoContent>(project.sequences.at(main_seq_id).tracks[0].clips[0].content)
            .media;
    project.sequences.at(main_seq_id).tracks[0].clips[1].content =
        model::VideoContent{video_media_id};

    auto ed_res = Editor::create(std::move(project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto v2_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[1].id;

    // Head trim V2 to 7s: start becomes 7s, duration becomes 3s, source_in advances by 2s
    TrimClip cmd{main_seq_id, v2_id, TrimEdge::Head, timeline_at_seconds(7), false};

    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& v_clips = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips;
    EXPECT_EQ(v_clips[1].start, timeline_at_seconds(7));
    EXPECT_EQ(v_clips[1].duration, duration_of_seconds(3));
    EXPECT_EQ(v_clips[1].source_in.ticks(), timeline_at_seconds(2).ticks());
}

TEST(TrimClipTest, RippleHeadTrimKeepsStartAndShiftsLaterClips) {
    core::UuidGenerator gen(7004ULL);
    auto project = model::test::build_valid_project(gen);
    const auto main_seq_id = project.main_sequence;

    const auto video_media_id =
        std::get<model::VideoContent>(project.sequences.at(main_seq_id).tracks[0].clips[0].content)
            .media;
    project.sequences.at(main_seq_id).tracks[0].clips[1].content =
        model::VideoContent{video_media_id};

    auto ed_res = Editor::create(std::move(project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto v2_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[1].id;

    // Ripple head trim V2 to 7s:
    // start stays 5s, duration becomes 3s, later clips shift by -2s
    TrimClip cmd{main_seq_id, v2_id, TrimEdge::Head, timeline_at_seconds(7), true};

    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& v_clips = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips;
    EXPECT_EQ(v_clips[1].start, timeline_at_seconds(5));
    EXPECT_EQ(v_clips[1].duration, duration_of_seconds(3));
    EXPECT_EQ(v_clips[1].source_in.ticks(), timeline_at_seconds(2).ticks());

    // V3 (originally at 10s) shifts by -2s to 8s
    EXPECT_EQ(v_clips[2].start, timeline_at_seconds(8));
}

TEST(TrimClipTest, ExtendingTailWithoutRippleIntoV2FailsWithOverlap) {
    core::UuidGenerator gen(7005ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto v1_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].id;

    // Extend V1 tail from 5s to 7s (into V2 at 5..10s) without ripple
    TrimClip cmd{main_seq_id, v1_id, TrimEdge::Tail, timeline_at_seconds(7), false};

    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
    EXPECT_NE(res.error().message().find("overlap"), std::string::npos);
}

TEST(TrimClipTest, ExtendingTailBeyondMediaDurationFailsWithInvalidArgument) {
    core::UuidGenerator gen(7006ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto v1_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].id;

    // Remove V2 and V3 so timeline has space
    DeleteClips del_v2_v3{main_seq_id,
                          {editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[1].id,
                           editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[2].id},
                          true};
    ASSERT_TRUE(test::is_ok(editor.execute(del_v2_v3)));

    // Extend V1 tail to 65s (media duration is 60s)
    TrimClip cmd{main_seq_id, v1_id, TrimEdge::Tail, timeline_at_seconds(65), false};

    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
}

TEST(TrimClipTest, TrimmingToZeroLengthFails) {
    core::UuidGenerator gen(7007ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto v1_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].id;

    // Tail trim V1 to 0s (same as start)
    TrimClip cmd{main_seq_id, v1_id, TrimEdge::Tail, timeline_at_seconds(0), false};

    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
}

TEST(TrimClipTest, RippleBlockedBySameTrackOverlapFailsWithOverlap) {
    core::UuidGenerator gen(7008ULL);
    auto project = model::test::build_valid_project(gen);
    const auto main_seq_id = project.main_sequence;

    // Unlink V1 so it doesn't pull A1
    project.sequences.at(main_seq_id).tracks[0].clips[0].link_id = std::nullopt;
    project.sequences.at(main_seq_id).tracks[1].clips[0].link_id = std::nullopt;

    // Add clip C (12..14s) on audio track
    const auto video_media_id =
        std::get<model::VideoContent>(project.sequences.at(main_seq_id).tracks[0].clips[0].content)
            .media;

    model::Clip clip_c;
    clip_c.id = model::generate_id<model::ClipId>(gen);
    clip_c.start = timeline_at_seconds(6);
    clip_c.duration = duration_of_seconds(2);
    clip_c.content = model::AudioContent{video_media_id};
    project.sequences.at(main_seq_id).tracks[1].clips.push_back(clip_c);

    // Add clip D (13..15s) on audio track that already exists
    model::Clip clip_d;
    clip_d.id = model::generate_id<model::ClipId>(gen);
    clip_d.start = timeline_at_seconds(15);
    clip_d.duration = duration_of_seconds(2);
    clip_d.content = model::AudioContent{video_media_id};
    project.sequences.at(main_seq_id).tracks[1].clips.push_back(clip_d);

    auto ed_res = Editor::create(std::move(project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto v1_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].id;

    // Ripple shorten V1 to 3s (-2s): clip C (6..8s) would shift to 4..6s,
    // which overlaps the unlinked audio clip A1 (0..5s) that stays in place
    TrimClip cmd{main_seq_id, v1_id, TrimEdge::Tail, timeline_at_seconds(3), true};

    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
    EXPECT_NE(res.error().message().find("overlap"), std::string::npos);
}

TEST(TrimClipTest, IgnoreLinksLeavesPartnerAlone) {
    core::UuidGenerator gen(7009ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto v1_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].id;

    // Tail trim V1 to 3s with ignore_links = true
    TrimClip cmd{main_seq_id,
                 v1_id,
                 TrimEdge::Tail,
                 timeline_at_seconds(3),
                 false,
                 RippleScope::AllUnlockedTracks,
                 true};

    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& v_clips = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips;
    const auto& a_clips = editor.snapshot()->sequences.at(main_seq_id).tracks[1].clips;

    EXPECT_EQ(v_clips[0].duration, duration_of_seconds(3));
    // A1 left alone at 5s
    EXPECT_EQ(a_clips[0].duration, duration_of_seconds(5));
}

TEST(TrimClipTest, LockedTrackFailsWithLockedMessage) {
    core::UuidGenerator gen(7010ULL);
    auto project = model::test::build_valid_project(gen);
    const auto main_seq_id = project.main_sequence;
    project.sequences.at(main_seq_id).tracks[0].locked = true;

    auto ed_res = Editor::create(std::move(project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto v1_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].id;

    TrimClip cmd{main_seq_id, v1_id, TrimEdge::Tail, timeline_at_seconds(3), false};

    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
    EXPECT_NE(res.error().message().find("locked"), std::string::npos);
}

TEST(TrimClipTest, TrimChangingNothingReturnsEmptyChangeSet) {
    core::UuidGenerator gen(7011ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto v1_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].id;

    // Trim V1 tail to 5s (its current end)
    TrimClip cmd{main_seq_id, v1_id, TrimEdge::Tail, timeline_at_seconds(5), false};

    auto build_res = cmd.build(*editor.snapshot(), gen);
    ASSERT_TRUE(test::is_ok(build_res));
    EXPECT_TRUE(build_res.value().empty());

    auto exec_res = editor.execute(cmd);
    ASSERT_TRUE(test::is_ok(exec_res));
    EXPECT_FALSE(editor.can_undo());
}

TEST(TrimClipTest, TrimClipExtremeHeadTrimFailsWithoutThrow) {
    core::UuidGenerator gen(7012ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto v1_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].id;

    const auto extreme_edge = model::TimelineTime::from_ticks(
        std::numeric_limits<std::int64_t>::min() + core::kTicksPerSecond);
    TrimClip cmd{main_seq_id, v1_id, TrimEdge::Head, extreme_edge, false};

    core::Result<EditReceipt> res;
    EXPECT_NO_THROW({ res = editor.execute(cmd); });
    EXPECT_FALSE(res.has_value());
}

TEST(TrimClipTest, AudioFadeHeadTrimSatisfiesValidate) {
    core::UuidGenerator gen(7013ULL);
    auto project = model::test::build_valid_project(gen);
    const auto main_seq_id = project.main_sequence;
    auto& a1 = project.sequences.at(main_seq_id).tracks[1].clips[0];
    auto& a1_content = std::get<model::AudioContent>(a1.content);
    a1_content.fade_in = duration_of_seconds(1);
    a1_content.fade_out = duration_of_seconds(1);

    auto ed_res = Editor::create(std::move(project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto a1_id = editor.snapshot()->sequences.at(main_seq_id).tracks[1].clips[0].id;

    TrimClip cmd{main_seq_id,
                 a1_id,
                 TrimEdge::Head,
                 timeline_at_seconds(4),
                 false,
                 RippleScope::AllUnlockedTracks,
                 true};

    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& a_clips = editor.snapshot()->sequences.at(main_seq_id).tracks[1].clips;
    EXPECT_EQ(a_clips[0].start, timeline_at_seconds(4));
    EXPECT_EQ(a_clips[0].duration, duration_of_seconds(1));
}

TEST(TrimClipTest, AudioFadeTailTrimSatisfiesValidate) {
    core::UuidGenerator gen(7014ULL);
    auto project = model::test::build_valid_project(gen);
    const auto main_seq_id = project.main_sequence;
    auto& a1 = project.sequences.at(main_seq_id).tracks[1].clips[0];
    auto& a1_content = std::get<model::AudioContent>(a1.content);
    a1_content.fade_in = duration_of_seconds(1);
    a1_content.fade_out = duration_of_seconds(1);

    auto ed_res = Editor::create(std::move(project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto a1_id = editor.snapshot()->sequences.at(main_seq_id).tracks[1].clips[0].id;

    TrimClip cmd{main_seq_id,
                 a1_id,
                 TrimEdge::Tail,
                 timeline_at_seconds(1),
                 false,
                 RippleScope::AllUnlockedTracks,
                 true};

    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& a_clips = editor.snapshot()->sequences.at(main_seq_id).tracks[1].clips;
    EXPECT_EQ(a_clips[0].start, timeline_at_seconds(0));
    EXPECT_EQ(a_clips[0].duration, duration_of_seconds(1));
}

TEST(TrimClipTest, HeadTrimV1WithIgnoreLinksNormalAndSpeed2) {
    // Normal speed
    {
        core::UuidGenerator gen(7015ULL);
        auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
        ASSERT_TRUE(test::is_ok(ed_res));
        Editor& editor = *ed_res.value();

        const auto main_seq_id = editor.snapshot()->main_sequence;
        const auto v1_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].id;

        TrimClip cmd{main_seq_id,
                     v1_id,
                     TrimEdge::Head,
                     timeline_at_seconds(2),
                     false,
                     RippleScope::AllUnlockedTracks,
                     true};

        auto rt_res = test::round_trip(editor, cmd);
        ASSERT_TRUE(test::is_ok(rt_res));

        const auto& v_clips = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips;
        EXPECT_EQ(v_clips[0].start, timeline_at_seconds(2));
        EXPECT_EQ(v_clips[0].duration, duration_of_seconds(3));
        EXPECT_EQ(v_clips[0].source_in.ticks(), timeline_at_seconds(2).ticks());
    }
    // Speed 2/1
    {
        core::UuidGenerator gen(7016ULL);
        auto project = model::test::build_valid_project(gen);
        const auto main_seq_id = project.main_sequence;
        project.sequences.at(main_seq_id).tracks[0].clips[0].speed =
            model::Speed::create(2, 1).value();

        auto ed_res = Editor::create(std::move(project), gen);
        ASSERT_TRUE(test::is_ok(ed_res));
        Editor& editor = *ed_res.value();

        const auto v1_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].id;

        TrimClip cmd{main_seq_id,
                     v1_id,
                     TrimEdge::Head,
                     timeline_at_seconds(2),
                     false,
                     RippleScope::AllUnlockedTracks,
                     true};

        auto rt_res = test::round_trip(editor, cmd);
        ASSERT_TRUE(test::is_ok(rt_res));

        const auto& v_clips = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips;
        EXPECT_EQ(v_clips[0].start, timeline_at_seconds(2));
        EXPECT_EQ(v_clips[0].duration, duration_of_seconds(3));
        EXPECT_EQ(v_clips[0].source_in.ticks(), timeline_at_seconds(4).ticks());
    }
}

TEST(TrimClipTest, RippleHeadTrimV1WithDefaultLinksShiftsLaterClipsAndTrimsA1) {
    core::UuidGenerator gen(7017ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto v1_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].id;

    TrimClip cmd{main_seq_id, v1_id, TrimEdge::Head, timeline_at_seconds(2), true};

    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& v_clips = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips;
    const auto& a_clips = editor.snapshot()->sequences.at(main_seq_id).tracks[1].clips;

    // V1: start stays 0, duration 3s, source_in = 2s
    EXPECT_EQ(v_clips[0].start, timeline_at_seconds(0));
    EXPECT_EQ(v_clips[0].duration, duration_of_seconds(3));
    EXPECT_EQ(v_clips[0].source_in.ticks(), timeline_at_seconds(2).ticks());

    // A1: trimmed by same delta: start 0, duration 3s, source_in +2s
    EXPECT_EQ(a_clips[0].start, timeline_at_seconds(0));
    EXPECT_EQ(a_clips[0].duration, duration_of_seconds(3));
    EXPECT_EQ(a_clips[0].source_in.ticks(), timeline_at_seconds(2).ticks());

    // V2 becomes 3..8s
    EXPECT_EQ(v_clips[1].start, timeline_at_seconds(3));
    EXPECT_EQ(v_clips[1].duration, duration_of_seconds(5));

    // V3 becomes 8..13s
    EXPECT_EQ(v_clips[2].start, timeline_at_seconds(8));
    EXPECT_EQ(v_clips[2].duration, duration_of_seconds(5));
}

TEST(TrimClipTest, RippleTailExtensionV1ShiftsV2AndV3) {
    core::UuidGenerator gen(7018ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto v1_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].id;

    // Extend V1 tail from 5s to 8s with ripple
    TrimClip cmd{main_seq_id, v1_id, TrimEdge::Tail, timeline_at_seconds(8), true};

    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& v_clips = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips;

    // V1: duration 8s
    EXPECT_EQ(v_clips[0].start, timeline_at_seconds(0));
    EXPECT_EQ(v_clips[0].duration, duration_of_seconds(8));

    // V2: shifts +3s (from 5..10s to 8..13s)
    EXPECT_EQ(v_clips[1].start, timeline_at_seconds(8));
    EXPECT_EQ(v_clips[1].duration, duration_of_seconds(5));

    // V3: shifts +3s (from 10..15s to 13..18s)
    EXPECT_EQ(v_clips[2].start, timeline_at_seconds(13));
    EXPECT_EQ(v_clips[2].duration, duration_of_seconds(5));
}

}  // namespace
}  // namespace nxtcut::commands
