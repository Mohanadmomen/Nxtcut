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

TEST(MoveClipsTest, MoveV3to20sSucceeds) {
    core::UuidGenerator gen(4001ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto v_track_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].id;
    const auto v3_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[2].id;

    MoveClips cmd{main_seq_id, {{v3_id, timeline_at_seconds(20), v_track_id}}};

    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& clips = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips;
    ASSERT_EQ(clips.size(), 3U);
    EXPECT_EQ(clips[2].id, v3_id);
    EXPECT_EQ(clips[2].start, timeline_at_seconds(20));
}

TEST(MoveClipsTest, MoveV1to3sFailsWithOverlapMessage) {
    core::UuidGenerator gen(4002ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto v_track_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].id;
    const auto v1_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].id;

    // V1 is 5s. Moving to 3s makes it 3..8s, which overlaps V2 (5..10s)
    MoveClips cmd{main_seq_id, {{v1_id, timeline_at_seconds(3), v_track_id}}};

    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
    EXPECT_NE(res.error().message().find("overlap"), std::string::npos);
}

TEST(MoveClipsTest, MoveByZeroReturnsEmptyChangeSet) {
    core::UuidGenerator gen(4003ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto v_track_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].id;
    const auto v1_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].id;

    MoveClips cmd{main_seq_id, {{v1_id, timeline_at_seconds(0), v_track_id}}};

    auto build_res = cmd.build(*editor.snapshot(), gen);
    ASSERT_TRUE(test::is_ok(build_res));
    EXPECT_TRUE(build_res.value().empty());

    // Executing empty changeset is a no-op that publishes no history step
    auto exec_res = editor.execute(cmd);
    ASSERT_TRUE(test::is_ok(exec_res));
    EXPECT_FALSE(editor.can_undo());
}

TEST(MoveClipsTest, LinkedV1andA1MoveTogetherUnlessIgnoreLinks) {
    core::UuidGenerator gen(4004ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto v_track_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].id;
    const auto v1_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].id;

    // First delete V2 and V3 so V1 has room to move to 2s
    DeleteClips del_v2_v3{main_seq_id,
                          {editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[1].id,
                           editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[2].id},
                          true};
    ASSERT_TRUE(test::is_ok(editor.execute(del_v2_v3)));

    // 1. Move V1 by +2s (to 2s) with ignore_links = false: A1 moves too!
    MoveClips cmd_linked{main_seq_id, {{v1_id, timeline_at_seconds(2), v_track_id}}, false};
    ASSERT_TRUE(test::is_ok(test::round_trip(editor, cmd_linked)));
    EXPECT_EQ(editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].start,
              timeline_at_seconds(2));
    EXPECT_EQ(editor.snapshot()->sequences.at(main_seq_id).tracks[1].clips[0].start,
              timeline_at_seconds(2));

    // Undo to baseline
    static_cast<void>(editor.undo());

    // 2. Move V1 by +2s with ignore_links = true: A1 stays!
    MoveClips cmd_unlinked{main_seq_id, {{v1_id, timeline_at_seconds(2), v_track_id}}, true};
    ASSERT_TRUE(test::is_ok(test::round_trip(editor, cmd_unlinked)));
    EXPECT_EQ(editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].start,
              timeline_at_seconds(2));
    EXPECT_EQ(editor.snapshot()->sequences.at(main_seq_id).tracks[1].clips[0].start,
              timeline_at_seconds(0));
}

TEST(MoveClipsTest, MoveToTrackOfWrongKindGivesInvalidArgument) {
    core::UuidGenerator gen(4005ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto a_track_id = editor.snapshot()->sequences.at(main_seq_id).tracks[1].id;
    const auto v1_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].id;

    MoveClips cmd{main_seq_id, {{v1_id, timeline_at_seconds(20), a_track_id}}};

    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
}

TEST(MoveClipsTest, NegativeStartGivesInvalidArgument) {
    core::UuidGenerator gen(4006ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto v_track_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].id;
    const auto v1_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].id;

    MoveClips cmd{main_seq_id,
                  {{v1_id, model::TimelineTime::from_ticks(-core::kTicksPerSecond), v_track_id}}};

    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
}

TEST(MoveClipsTest, LockedSourceTrackFailsWithLockedMessage) {
    core::UuidGenerator gen(4007ULL);
    auto project = model::test::build_valid_project(gen);
    const auto main_seq_id = project.main_sequence;
    project.sequences.at(main_seq_id).tracks[0].locked = true;

    auto ed_res = Editor::create(std::move(project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto v_track_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].id;
    const auto v3_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[2].id;

    MoveClips cmd{main_seq_id, {{v3_id, timeline_at_seconds(20), v_track_id}}};

    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
    EXPECT_NE(res.error().message().find("locked"), std::string::npos);
}

TEST(MoveClipsTest, MoveV2andV3TogetherBy1sSucceeds) {
    core::UuidGenerator gen(4008ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto v_track_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].id;
    const auto v2_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[1].id;
    const auto v3_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[2].id;

    // V2 from 5s to 6s; V3 from 10s to 11s
    MoveClips cmd{main_seq_id,
                  {{v2_id, timeline_at_seconds(6), v_track_id},
                   {v3_id, timeline_at_seconds(11), v_track_id}}};

    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& clips = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips;
    EXPECT_EQ(clips[1].start, timeline_at_seconds(6));
    EXPECT_EQ(clips[2].start, timeline_at_seconds(11));
}

TEST(MoveClipsTest, SwapLikeMovesSucceedWhenBothListed) {
    core::UuidGenerator gen(4009ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto v_track_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].id;
    const auto v1_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].id;
    const auto v2_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[1].id;

    // Both V1 and V2 are 5s duration. V1 is 0..5s, V2 is 5..10s.
    // Swap: V2 to 0s, V1 to 5s.
    // Unlink V1 first to test swap cleanly
    DeleteClips del_a1{
        main_seq_id, {editor.snapshot()->sequences.at(main_seq_id).tracks[1].clips[0].id}, true};
    ASSERT_TRUE(test::is_ok(editor.execute(del_a1)));

    MoveClips swap_cmd{
        main_seq_id,
        {{v2_id, timeline_at_seconds(0), v_track_id}, {v1_id, timeline_at_seconds(5), v_track_id}}};

    auto rt_res = test::round_trip(editor, swap_cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& clips = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips;
    EXPECT_EQ(clips[0].id, v2_id);
    EXPECT_EQ(clips[0].start, timeline_at_seconds(0));
    EXPECT_EQ(clips[1].id, v1_id);
    EXPECT_EQ(clips[1].start, timeline_at_seconds(5));
}

TEST(MoveClipsTest, SameClipListedTwiceGivesInvalidArgument) {
    core::UuidGenerator gen(4010ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto v_track_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].id;
    const auto v1_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].id;

    MoveClips cmd{main_seq_id,
                  {{v1_id, timeline_at_seconds(20), v_track_id},
                   {v1_id, timeline_at_seconds(25), v_track_id}}};

    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
}

TEST(MoveClipsTest, CrossTrackMoveOfAudioClipWorks) {
    core::UuidGenerator gen(4011ULL);
    auto project = model::test::build_valid_project(gen);
    const auto main_seq_id = project.main_sequence;

    // Add a 2nd audio track
    const auto a2_track_id = model::generate_id<model::TrackId>(gen);
    model::Track a2_track;
    a2_track.id = a2_track_id;
    a2_track.name = "A2";
    a2_track.kind = model::TrackKind::Audio;
    a2_track.enabled = true;
    a2_track.locked = false;
    project.sequences.at(main_seq_id).tracks.push_back(a2_track);

    auto ed_res = Editor::create(std::move(project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto a1_id = editor.snapshot()->sequences.at(main_seq_id).tracks[1].clips[0].id;

    MoveClips cmd{main_seq_id, {{a1_id, timeline_at_seconds(10), a2_track_id}}, true};

    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    EXPECT_TRUE(editor.snapshot()->sequences.at(main_seq_id).tracks[1].clips.empty());
    const auto& a2_clips = editor.snapshot()->sequences.at(main_seq_id).tracks[2].clips;
    ASSERT_EQ(a2_clips.size(), 1U);
    EXPECT_EQ(a2_clips[0].id, a1_id);
    EXPECT_EQ(a2_clips[0].start, timeline_at_seconds(10));
}

}  // namespace
}  // namespace nxtcut::commands
