#include <nxtcut/commands/editor.hpp>
#include <nxtcut/commands/timeline_commands.hpp>
#include <nxtcut/core/time.hpp>
#include <nxtcut/core/uuid.hpp>
#include <nxtcut/model/clip.hpp>
#include <nxtcut/model/ids.hpp>
#include <nxtcut/model/project.hpp>
#include <nxtcut/model/time_coords.hpp>

#include <gtest/gtest.h>

#include <nxtcut_test/assertions.hpp>
#include <nxtcut_test/model_fixtures.hpp>
#include "test_helper.hpp"

namespace nxtcut::commands {
namespace {

using model::test::duration_of_seconds;
using model::test::timeline_at_seconds;

TEST(CloseGapTest, ClosesGapAndShiftsOtherTracks) {
    core::UuidGenerator gen(9001ULL);
    auto project = model::test::build_valid_project(gen);
    const auto main_seq_id = project.main_sequence;

    // Build video track with clips 0..2s and 5..7s
    const auto video_media_id =
        std::get<model::VideoContent>(
            project.sequences.at(main_seq_id).tracks[0].clips[0].content)
            .media;

    project.sequences.at(main_seq_id).tracks[0].clips.clear();

    model::Clip c1;
    c1.id = model::generate_id<model::ClipId>(gen);
    c1.start = timeline_at_seconds(0);
    c1.duration = duration_of_seconds(2);
    c1.content = model::VideoContent{video_media_id};
    project.sequences.at(main_seq_id).tracks[0].clips.push_back(c1);

    model::Clip c2;
    c2.id = model::generate_id<model::ClipId>(gen);
    c2.start = timeline_at_seconds(5);
    c2.duration = duration_of_seconds(2);
    c2.content = model::VideoContent{video_media_id};
    project.sequences.at(main_seq_id).tracks[0].clips.push_back(c2);

    // Audio track has a clip at 8..10s (start >= 5s)
    project.sequences.at(main_seq_id).tracks[1].clips.clear();
    model::Clip a1;
    a1.id = model::generate_id<model::ClipId>(gen);
    a1.start = timeline_at_seconds(8);
    a1.duration = duration_of_seconds(2);
    a1.content = model::AudioContent{video_media_id};
    project.sequences.at(main_seq_id).tracks[1].clips.push_back(a1);

    auto ed_res = Editor::create(std::move(project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto v_track_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].id;

    // Gap on video track is 2..5s (length 3s). Close gap at 3s.
    // c2 closes to 2..4s (shifted by -3s)
    // a1 on audio track shifts by -3s from 8s to 5s (5..7s)
    CloseGap cmd{main_seq_id, v_track_id, timeline_at_seconds(3)};

    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& v_clips = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips;
    const auto& a_clips = editor.snapshot()->sequences.at(main_seq_id).tracks[1].clips;

    EXPECT_EQ(v_clips[0].start, timeline_at_seconds(0));
    EXPECT_EQ(v_clips[1].start, timeline_at_seconds(2));
    EXPECT_EQ(v_clips[1].duration, duration_of_seconds(2));

    EXPECT_EQ(a_clips[0].start, timeline_at_seconds(5));
}

TEST(CloseGapTest, AtInsideClipGivesInvalidArgument) {
    core::UuidGenerator gen(9002ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto v_track_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].id;

    // V1 is 0..5s. at = 2s is inside V1
    CloseGap cmd{main_seq_id, v_track_id, timeline_at_seconds(2)};

    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
}

TEST(CloseGapTest, NoClipAfterGapGivesInvalidArgument) {
    core::UuidGenerator gen(9003ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto v_track_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].id;

    // Last clip ends at 15s. at = 20s has no clip after it
    CloseGap cmd{main_seq_id, v_track_id, timeline_at_seconds(20)};

    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
}

TEST(CloseGapTest, LeadingGapShiftsToZero) {
    core::UuidGenerator gen(9004ULL);
    auto project = model::test::build_valid_project(gen);
    const auto main_seq_id = project.main_sequence;

    const auto video_media_id =
        std::get<model::VideoContent>(
            project.sequences.at(main_seq_id).tracks[0].clips[0].content)
            .media;

    // Track with single clip at 3..8s
    project.sequences.at(main_seq_id).tracks[0].clips.clear();
    model::Clip c;
    c.id = model::generate_id<model::ClipId>(gen);
    c.start = timeline_at_seconds(3);
    c.duration = duration_of_seconds(5);
    c.content = model::VideoContent{video_media_id};
    project.sequences.at(main_seq_id).tracks[0].clips.push_back(c);

    // Audio track clear
    project.sequences.at(main_seq_id).tracks[1].clips.clear();

    auto ed_res = Editor::create(std::move(project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto v_track_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].id;

    // Leading gap 0..3s, at = 1s -> shifts clip to 0s
    CloseGap cmd{main_seq_id, v_track_id, timeline_at_seconds(1)};

    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& v_clips = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips;
    EXPECT_EQ(v_clips[0].start, timeline_at_seconds(0));
}

TEST(CloseGapTest, LockedTrackFailsWithLockedMessage) {
    core::UuidGenerator gen(9005ULL);
    auto project = model::test::build_valid_project(gen);
    const auto main_seq_id = project.main_sequence;
    project.sequences.at(main_seq_id).tracks[0].locked = true;

    auto ed_res = Editor::create(std::move(project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto v_track_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].id;

    CloseGap cmd{main_seq_id, v_track_id, timeline_at_seconds(2)};

    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
    EXPECT_NE(res.error().message().find("locked"), std::string::npos);
}

TEST(CloseGapTest, OverlapOnAnotherTrackFailsWithOverlapMessage) {
    core::UuidGenerator gen(9006ULL);
    auto project = model::test::build_valid_project(gen);
    const auto main_seq_id = project.main_sequence;

    const auto video_media_id =
        std::get<model::VideoContent>(
            project.sequences.at(main_seq_id).tracks[0].clips[0].content)
            .media;

    // Video track with gap 2..5s (clips at 0..2s and 5..7s)
    project.sequences.at(main_seq_id).tracks[0].clips.clear();
    model::Clip v1;
    v1.id = model::generate_id<model::ClipId>(gen);
    v1.start = timeline_at_seconds(0);
    v1.duration = duration_of_seconds(2);
    v1.content = model::VideoContent{video_media_id};
    project.sequences.at(main_seq_id).tracks[0].clips.push_back(v1);

    model::Clip v2;
    v2.id = model::generate_id<model::ClipId>(gen);
    v2.start = timeline_at_seconds(5);
    v2.duration = duration_of_seconds(2);
    v2.content = model::VideoContent{video_media_id};
    project.sequences.at(main_seq_id).tracks[0].clips.push_back(v2);

    // Audio track has clip at 1..4s and clip at 6..8s
    // Shifting clip at 6..8s by -3s makes it 3..5s, colliding with 1..4s!
    project.sequences.at(main_seq_id).tracks[1].clips.clear();
    model::Clip a1;
    a1.id = model::generate_id<model::ClipId>(gen);
    a1.start = timeline_at_seconds(1);
    a1.duration = duration_of_seconds(3);
    a1.content = model::AudioContent{video_media_id};
    project.sequences.at(main_seq_id).tracks[1].clips.push_back(a1);

    model::Clip a2;
    a2.id = model::generate_id<model::ClipId>(gen);
    a2.start = timeline_at_seconds(6);
    a2.duration = duration_of_seconds(2);
    a2.content = model::AudioContent{video_media_id};
    project.sequences.at(main_seq_id).tracks[1].clips.push_back(a2);

    auto ed_res = Editor::create(std::move(project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto v_track_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].id;

    CloseGap cmd{main_seq_id, v_track_id, timeline_at_seconds(3)};

    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
    EXPECT_NE(res.error().message().find("overlap"), std::string::npos);
}

}  // namespace
}  // namespace nxtcut::commands
