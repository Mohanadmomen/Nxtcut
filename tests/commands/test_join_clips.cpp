#include <nxtcut/commands/editor.hpp>
#include <nxtcut/commands/timeline_commands.hpp>
#include <nxtcut/commands/track_commands.hpp>
#include <nxtcut/core/time.hpp>
#include <nxtcut/core/uuid.hpp>
#include <nxtcut/model/clip.hpp>
#include <nxtcut/model/equality.hpp>
#include <nxtcut/model/ids.hpp>
#include <nxtcut/model/project.hpp>
#include <nxtcut/model/speed.hpp>
#include <nxtcut/model/time_coords.hpp>

#include <optional>
#include <string>
#include <variant>

#include <gtest/gtest.h>

#include <nxtcut_test/assertions.hpp>
#include <nxtcut_test/model_fixtures.hpp>
#include "test_helper.hpp"

namespace nxtcut::commands {
namespace {

using model::test::duration_of_seconds;
using model::test::timeline_at_seconds;

TEST(JoinClipsTest, SplitV1ThenJoinRestoresIdenticalClip) {
    core::UuidGenerator gen(1101ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto orig_v1 = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0];

    // Split V1 at 2s (ignore_links = true to split only V1)
    SplitClip split_cmd{main_seq_id, orig_v1.id, timeline_at_seconds(2), true};
    ASSERT_TRUE(test::is_ok(editor.execute(split_cmd)));

    const auto left_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].id;
    const auto right_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[1].id;

    // Join left and right
    JoinClips join_cmd{main_seq_id, {{left_id, right_id}}};

    auto rt_res = test::round_trip(editor, join_cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& joined_v1 = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0];
    EXPECT_TRUE(model::identical(joined_v1, orig_v1));
}

TEST(JoinClipsTest, JoinAudioPairAfterSplitRestoresFades) {
    core::UuidGenerator gen(1102ULL);
    auto project = model::test::build_valid_project(gen);
    const auto main_seq_id = project.main_sequence;

    // Set fades on A1
    auto& audio =
        std::get<model::AudioContent>(project.sequences.at(main_seq_id).tracks[1].clips[0].content);
    audio.fade_in = duration_of_seconds(1);
    audio.fade_out = duration_of_seconds(1);

    auto ed_res = Editor::create(std::move(project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto orig_a1 = editor.snapshot()->sequences.at(main_seq_id).tracks[1].clips[0];

    // Split A1 at 2s
    SplitClip split_cmd{main_seq_id, orig_a1.id, timeline_at_seconds(2), true};
    ASSERT_TRUE(test::is_ok(editor.execute(split_cmd)));

    const auto left_id = editor.snapshot()->sequences.at(main_seq_id).tracks[1].clips[0].id;
    const auto right_id = editor.snapshot()->sequences.at(main_seq_id).tracks[1].clips[1].id;

    // Join
    JoinClips join_cmd{main_seq_id, {{left_id, right_id}}};

    auto rt_res = test::round_trip(editor, join_cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& joined_a1 = editor.snapshot()->sequences.at(main_seq_id).tracks[1].clips[0];
    EXPECT_TRUE(model::identical(joined_a1, orig_a1));
}

TEST(JoinClipsTest, MismatchedPropertiesGiveInvalidArgument) {
    core::UuidGenerator gen(1103ULL);
    auto project = model::test::build_valid_project(gen);
    const auto main_seq_id = project.main_sequence;

    // Create two video clips on video track
    const auto video_media_id =
        std::get<model::VideoContent>(
            project.sequences.at(main_seq_id).tracks[0].clips[0].content)
            .media;

    project.sequences.at(main_seq_id).tracks[0].clips.clear();
    project.sequences.at(main_seq_id).tracks[1].clips[0].link_id = std::nullopt;
    model::Clip c1;
    c1.id = model::generate_id<model::ClipId>(gen);
    c1.start = timeline_at_seconds(0);
    c1.duration = duration_of_seconds(5);
    c1.source_in = model::SourceTime::zero();
    c1.content = model::VideoContent{video_media_id};

    model::Clip c2;
    c2.id = model::generate_id<model::ClipId>(gen);
    c2.start = timeline_at_seconds(5);
    c2.duration = duration_of_seconds(5);
    c2.source_in = model::SourceTime::from_ticks(timeline_at_seconds(5).ticks());
    c2.content = model::VideoContent{video_media_id};

    // 1. Non-adjacent clips
    {
        model::Project p = project;
        c2.start = timeline_at_seconds(6);  // not adjacent
        p.sequences.at(main_seq_id).tracks[0].clips = {c1, c2};
        core::UuidGenerator local_gen(11031ULL);
        auto ed = Editor::create(std::move(p), local_gen).value();
        JoinClips cmd{main_seq_id, {{c1.id, c2.id}}};
        auto res = ed->execute(cmd);
        EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
    }

    // 2. Different media
    {
        model::Project p = project;
        c2.start = timeline_at_seconds(5);
        c2.source_in = model::SourceTime::from_ticks(timeline_at_seconds(5).ticks());
        const auto video2_id = model::generate_id<model::MediaId>(gen);
        model::MediaAsset asset2 = p.media.at(video_media_id);
        asset2.id = video2_id;
        p.media[video2_id] = asset2;
        c2.content = model::VideoContent{video2_id};
        p.sequences.at(main_seq_id).tracks[0].clips = {c1, c2};
        core::UuidGenerator local_gen(11036ULL);
        auto ed = Editor::create(std::move(p), local_gen).value();
        JoinClips cmd{main_seq_id, {{c1.id, c2.id}}};
        auto res = ed->execute(cmd);
        EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
    }

    // 3. Non-contiguous source
    {
        model::Project p = project;
	c2.content = model::VideoContent{video_media_id};
        c2.start = timeline_at_seconds(5);
        c2.source_in = model::SourceTime::from_ticks(timeline_at_seconds(10).ticks());  // gap in source
        p.sequences.at(main_seq_id).tracks[0].clips = {c1, c2};
        core::UuidGenerator local_gen(11032ULL);
        auto ed = Editor::create(std::move(p), local_gen).value();
        JoinClips cmd{main_seq_id, {{c1.id, c2.id}}};
        auto res = ed->execute(cmd);
        EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
    }

    // 4. Different speed
    {
        model::Project p = project;
        c2.start = timeline_at_seconds(5);
        c2.source_in = model::SourceTime::from_ticks(timeline_at_seconds(5).ticks());
        c2.speed = model::Speed::create(2, 1).value();
        p.sequences.at(main_seq_id).tracks[0].clips = {c1, c2};
        core::UuidGenerator local_gen(11033ULL);
        auto ed = Editor::create(std::move(p), local_gen).value();
        JoinClips cmd{main_seq_id, {{c1.id, c2.id}}};
        auto res = ed->execute(cmd);
        EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
    }

    // 5. Different transform
    {
        model::Project p = project;
        c2.start = timeline_at_seconds(5);
        c2.source_in = model::SourceTime::from_ticks(timeline_at_seconds(5).ticks());
        c2.speed = model::Speed::normal();
        c2.transform.opacity.set_constant(0.5);
        p.sequences.at(main_seq_id).tracks[0].clips = {c1, c2};
        core::UuidGenerator local_gen(11034ULL);
        auto ed = Editor::create(std::move(p), local_gen).value();
        JoinClips cmd{main_seq_id, {{c1.id, c2.id}}};
        auto res = ed->execute(cmd);
        EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
    }

    // 6. Different tracks
    {
        model::Project p = project;
        c2.start = timeline_at_seconds(5);
        c2.source_in = model::SourceTime::from_ticks(timeline_at_seconds(5).ticks());
        c2.speed = model::Speed::normal();
        c2.transform = c1.transform;
        c2.content = model::AudioContent{video_media_id};
        p.sequences.at(main_seq_id).tracks[0].clips = {c1};
        // Put c2 on track 1 (Audio track kind doesn't matter for this test, but let's make a video track)
        p.sequences.at(main_seq_id).tracks[1].clips = {c2};
        core::UuidGenerator local_gen(11035ULL);
        auto ed = Editor::create(std::move(p), local_gen).value();
        JoinClips cmd{main_seq_id, {{c1.id, c2.id}}};
        auto res = ed->execute(cmd);
        EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
    }
}

TEST(JoinClipsTest, TextOrCompoundGivesInvalidArgument) {
    core::UuidGenerator gen(1104ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto v3_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[2].id;

    // V3 is a compound clip. Split then join
    SplitClip split_cmd{main_seq_id, v3_id, timeline_at_seconds(12), true};
    ASSERT_TRUE(test::is_ok(editor.execute(split_cmd)));

    const auto c1_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[2].id;
    const auto c2_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[3].id;

    JoinClips join_cmd{main_seq_id, {{c1_id, c2_id}}};
    auto res = editor.execute(join_cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
}

TEST(JoinClipsTest, SameClipInTwoPairsGivesInvalidArgument) {
    core::UuidGenerator gen(1105ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto v1_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].id;
    const auto v2_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[1].id;

    JoinClips cmd{main_seq_id, {{v1_id, v2_id}, {v2_id, v1_id}}};
    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
}

TEST(JoinClipsTest, JoiningVideoPairLeavesAudioHalvesWithLinkCleared) {
    core::UuidGenerator gen(1106ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto v1_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].id;

    // Split V1 with ignore_links = false (splits both V1 and A1)
    SplitClip split_cmd{main_seq_id, v1_id, timeline_at_seconds(2), false};
    ASSERT_TRUE(test::is_ok(editor.execute(split_cmd)));

    const auto v_left = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].id;
    const auto v_right = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[1].id;

    // Join only the video pair
    JoinClips join_cmd{main_seq_id, {{v_left, v_right}}};

    auto rt_res = test::round_trip(editor, join_cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    // The audio track still has two halves
    const auto& a_clips = editor.snapshot()->sequences.at(main_seq_id).tracks[1].clips;
    ASSERT_EQ(a_clips.size(), 2U);
    // Link repair: right half audio had a link with right half video, but right half video was removed!
    // So right half audio has fewer than 2 members -> cleared!
    EXPECT_FALSE(a_clips[1].link_id.has_value());
}

TEST(JoinClipsTest, LockedTrackFailsWithLockedMessage) {
    core::UuidGenerator gen(1107ULL);
    auto project = model::test::build_valid_project(gen);
    const auto main_seq_id = project.main_sequence;

    auto ed_res = Editor::create(std::move(project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto v1_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].id;

    SplitClip split_cmd{main_seq_id, v1_id, timeline_at_seconds(2), true};
    ASSERT_TRUE(test::is_ok(editor.execute(split_cmd)));

    const auto v_left = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].id;
    const auto v_right = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[1].id;

    // Lock track
    SetTrackProperties lock_cmd{main_seq_id,
                                editor.snapshot()->sequences.at(main_seq_id).tracks[0].id,
                                std::nullopt, std::nullopt, true};
    ASSERT_TRUE(test::is_ok(editor.execute(lock_cmd)));

    JoinClips join_cmd{main_seq_id, {{v_left, v_right}}};
    auto res = editor.execute(join_cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
    EXPECT_NE(res.error().message().find("locked"), std::string::npos);
}

}  // namespace
}  // namespace nxtcut::commands
