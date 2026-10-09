#include <nxtcut/commands/editor.hpp>
#include <nxtcut/commands/timeline_commands.hpp>
#include <nxtcut/core/time.hpp>
#include <nxtcut/core/uuid.hpp>
#include <nxtcut/model/clip.hpp>
#include <nxtcut/model/ids.hpp>
#include <nxtcut/model/project.hpp>
#include <nxtcut/model/time_coords.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "test_helper.hpp"
#include <nxtcut_test/assertions.hpp>
#include <nxtcut_test/model_fixtures.hpp>

namespace nxtcut::commands {
namespace {

using model::test::duration_of_seconds;
using model::test::timeline_at_seconds;

TEST(RippleDeleteClipsTest, RippleDeleteV2ShiftsV3to5s) {
    core::UuidGenerator gen(8001ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto v2_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[1].id;

    // V2 is 5..10s. Ripple delete shifts V3 (10..15s) to 5..10s
    RippleDeleteClips cmd{main_seq_id, {v2_id}};

    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& v_clips = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips;
    ASSERT_EQ(v_clips.size(), 2U);
    EXPECT_EQ(v_clips[0].start, timeline_at_seconds(0));
    EXPECT_EQ(v_clips[1].start, timeline_at_seconds(5));
    EXPECT_EQ(v_clips[1].duration, duration_of_seconds(5));
}

TEST(RippleDeleteClipsTest, ClipOnAnotherUnlockedTrackShifts) {
    core::UuidGenerator gen(8002ULL);
    auto project = model::test::build_valid_project(gen);
    const auto main_seq_id = project.main_sequence;

    const auto video_media_id =
        std::get<model::VideoContent>(project.sequences.at(main_seq_id).tracks[0].clips[0].content)
            .media;

    // Add clip on audio track at 12..14s
    model::Clip a_clip;
    a_clip.id = model::generate_id<model::ClipId>(gen);
    a_clip.start = timeline_at_seconds(12);
    a_clip.duration = duration_of_seconds(2);
    a_clip.content = model::AudioContent{video_media_id};
    project.sequences.at(main_seq_id).tracks[1].clips.push_back(a_clip);

    auto ed_res = Editor::create(std::move(project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto v2_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[1].id;

    // Ripple delete V2 (5..10s)
    // Clip at 12..14s shifts earlier by 5s -> 7..9s
    RippleDeleteClips cmd{main_seq_id, {v2_id}};

    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& a_clips = editor.snapshot()->sequences.at(main_seq_id).tracks[1].clips;
    ASSERT_EQ(a_clips.size(), 2U);
    EXPECT_EQ(a_clips[1].start, timeline_at_seconds(7));
    EXPECT_EQ(a_clips[1].duration, duration_of_seconds(2));
}

TEST(RippleDeleteClipsTest, LockedTrackHoldingClipsIsUntouched) {
    core::UuidGenerator gen(8003ULL);
    auto project = model::test::build_valid_project(gen);
    const auto main_seq_id = project.main_sequence;

    const auto video_media_id =
        std::get<model::VideoContent>(project.sequences.at(main_seq_id).tracks[0].clips[0].content)
            .media;

    // Add 3rd track (Audio, locked) holding clip at 12..14s
    const auto a2_track_id = model::generate_id<model::TrackId>(gen);
    model::Track a2_track;
    a2_track.id = a2_track_id;
    a2_track.name = "A2";
    a2_track.kind = model::TrackKind::Audio;
    a2_track.enabled = true;
    a2_track.locked = true;

    model::Clip a2_clip;
    a2_clip.id = model::generate_id<model::ClipId>(gen);
    a2_clip.start = timeline_at_seconds(12);
    a2_clip.duration = duration_of_seconds(2);
    a2_clip.content = model::AudioContent{video_media_id};
    a2_track.clips.push_back(a2_clip);
    project.sequences.at(main_seq_id).tracks.push_back(a2_track);

    auto ed_res = Editor::create(std::move(project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto v2_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[1].id;

    // Ripple delete V2: locked track clips at 12..14s are untouched!
    RippleDeleteClips cmd{main_seq_id, {v2_id}};

    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& a2_clips = editor.snapshot()->sequences.at(main_seq_id).tracks[2].clips;
    EXPECT_EQ(a2_clips[0].start, timeline_at_seconds(12));
}

TEST(RippleDeleteClipsTest, DeletingLinkedV1ShiftsV2andV3byMinus5s) {
    core::UuidGenerator gen(8004ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto v1_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].id;

    // Deleting V1 (linked to A1) deletes both and closes 0..5s
    // V2 (5..10s) shifts to 0..5s; V3 (10..15s) shifts to 5..10s
    RippleDeleteClips cmd{main_seq_id, {v1_id}, RippleScope::AllUnlockedTracks, false};

    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& v_clips = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips;
    const auto& a_clips = editor.snapshot()->sequences.at(main_seq_id).tracks[1].clips;

    EXPECT_TRUE(a_clips.empty());
    ASSERT_EQ(v_clips.size(), 2U);
    EXPECT_EQ(v_clips[0].start, timeline_at_seconds(0));
    EXPECT_EQ(v_clips[0].duration, duration_of_seconds(5));
    EXPECT_EQ(v_clips[1].start, timeline_at_seconds(5));
    EXPECT_EQ(v_clips[1].duration, duration_of_seconds(5));
}

TEST(RippleDeleteClipsTest, BlockedSameTrackOverlapFailsWithOverlapMessage) {
    core::UuidGenerator gen(8005ULL);
    auto project = model::test::build_valid_project(gen);
    const auto main_seq_id = project.main_sequence;

    // Unlink V1 so it doesn't delete A1
    project.sequences.at(main_seq_id).tracks[0].clips[0].link_id = std::nullopt;
    project.sequences.at(main_seq_id).tracks[1].clips[0].link_id = std::nullopt;

    // Delete A1 at 0..5s to set up audio track cleanly
    project.sequences.at(main_seq_id).tracks[1].clips.clear();

    const auto video_media_id =
        std::get<model::VideoContent>(project.sequences.at(main_seq_id).tracks[0].clips[0].content)
            .media;

    // Clip C at 4..7s on audio track
    model::Clip clip_c;
    clip_c.id = model::generate_id<model::ClipId>(gen);
    clip_c.start = timeline_at_seconds(4);
    clip_c.duration = duration_of_seconds(3);
    clip_c.content = model::AudioContent{video_media_id};
    project.sequences.at(main_seq_id).tracks[1].clips.push_back(clip_c);

    // Clip D at 10..12s on audio track
    model::Clip clip_d;
    clip_d.id = model::generate_id<model::ClipId>(gen);
    clip_d.start = timeline_at_seconds(10);
    clip_d.duration = duration_of_seconds(2);
    clip_d.content = model::AudioContent{video_media_id};
    project.sequences.at(main_seq_id).tracks[1].clips.push_back(clip_d);

    auto ed_res = Editor::create(std::move(project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto v2_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[1].id;

    // Ripple delete V2 (5..10s):
    // C at 4..7s overlaps [5, 10) so it does NOT shift.
    // D at 10..12s shifts by -5s to 5..7s.
    // C (4..7s) and D (5..7s) collide! Fails with overlap!
    RippleDeleteClips cmd{main_seq_id, {v2_id}};

    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
    EXPECT_NE(res.error().message().find("overlap"), std::string::npos);
}

TEST(RippleDeleteClipsTest, DeletedClipOnLockedTrackFailsWithLockedMessage) {
    core::UuidGenerator gen(8006ULL);
    auto project = model::test::build_valid_project(gen);
    const auto main_seq_id = project.main_sequence;
    project.sequences.at(main_seq_id).tracks[0].locked = true;

    auto ed_res = Editor::create(std::move(project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto v2_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[1].id;

    RippleDeleteClips cmd{main_seq_id, {v2_id}};

    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
    EXPECT_NE(res.error().message().find("locked"), std::string::npos);
}

TEST(RippleDeleteClipsTest, TwoNonAdjacentDeletedClipsShiftBySum) {
    core::UuidGenerator gen(8007ULL);
    auto project = model::test::build_valid_project(gen);
    const auto main_seq_id = project.main_sequence;

    // Unlink V1
    project.sequences.at(main_seq_id).tracks[0].clips[0].link_id = std::nullopt;
    project.sequences.at(main_seq_id).tracks[1].clips[0].link_id = std::nullopt;

    // Add clip V4 at 20..25s
    const auto image_media_id =
        std::get<model::ImageContent>(project.sequences.at(main_seq_id).tracks[0].clips[1].content)
            .media;

    model::Clip v4;
    v4.id = model::generate_id<model::ClipId>(gen);
    v4.start = timeline_at_seconds(20);
    v4.duration = duration_of_seconds(5);
    v4.content = model::ImageContent{image_media_id};
    project.sequences.at(main_seq_id).tracks[0].clips.push_back(v4);

    auto ed_res = Editor::create(std::move(project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    // Delete V1 (0..5s, len 5s) and V3 (10..15s, len 5s)
    // Non-adjacent!
    // V2 (5..10s) starts at 5s >= 5s (end of V1), so shifts by 5s to 0..5s.
    // V4 (20..25s) starts at 20s >= 15s (end of V3), so shifts by (5s + 5s) = 10s to 10..15s!
    const auto v1_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].id;
    const auto v3_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[2].id;

    RippleDeleteClips cmd{main_seq_id, {v1_id, v3_id}, RippleScope::AllUnlockedTracks, true};

    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& v_clips = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips;
    ASSERT_EQ(v_clips.size(), 2U);
    EXPECT_EQ(v_clips[0].start, timeline_at_seconds(0));
    EXPECT_EQ(v_clips[0].duration, duration_of_seconds(5));
    EXPECT_EQ(v_clips[1].start, timeline_at_seconds(10));
    EXPECT_EQ(v_clips[1].duration, duration_of_seconds(5));
}

}  // namespace
}  // namespace nxtcut::commands
