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

TEST(DeleteClipsTest, DeleteV1RemovesV1andA1LeavingGap) {
    core::UuidGenerator gen(5001ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto v1_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].id;

    DeleteClips cmd{main_seq_id, {v1_id}, false};

    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& v_clips = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips;
    const auto& a_clips = editor.snapshot()->sequences.at(main_seq_id).tracks[1].clips;

    // V1 removed from video track, V2 and V3 stay in place (gap at 0..5s)
    ASSERT_EQ(v_clips.size(), 2U);
    EXPECT_EQ(v_clips[0].start, timeline_at_seconds(5));
    EXPECT_EQ(v_clips[1].start, timeline_at_seconds(10));

    // A1 removed from audio track
    EXPECT_TRUE(a_clips.empty());
}

TEST(DeleteClipsTest, DeleteV1WithIgnoreLinksLeavesA1WithClearedLink) {
    core::UuidGenerator gen(5002ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto v1_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].id;

    DeleteClips cmd{main_seq_id, {v1_id}, true};

    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& v_clips = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips;
    const auto& a_clips = editor.snapshot()->sequences.at(main_seq_id).tracks[1].clips;

    ASSERT_EQ(v_clips.size(), 2U);
    ASSERT_EQ(a_clips.size(), 1U);
    EXPECT_EQ(a_clips[0].start, timeline_at_seconds(0));
    EXPECT_FALSE(a_clips[0].link_id.has_value());
}

TEST(DeleteClipsTest, DeleteBothExplicitlyWorks) {
    core::UuidGenerator gen(5003ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto v1_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].id;
    const auto a1_id = editor.snapshot()->sequences.at(main_seq_id).tracks[1].clips[0].id;

    DeleteClips cmd{main_seq_id, {v1_id, a1_id}, false};

    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& v_clips = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips;
    const auto& a_clips = editor.snapshot()->sequences.at(main_seq_id).tracks[1].clips;

    EXPECT_EQ(v_clips.size(), 2U);
    EXPECT_TRUE(a_clips.empty());
}

TEST(DeleteClipsTest, UnknownIdGivesNotFound) {
    core::UuidGenerator gen(5004ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto fake_clip_id = model::generate_id<model::ClipId>(gen);

    DeleteClips cmd{main_seq_id, {fake_clip_id}, false};

    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::NotFound);
}

TEST(DeleteClipsTest, LockedTrackOfLinkedPartnerFailsWithLockedMessage) {
    core::UuidGenerator gen(5005ULL);
    auto project = model::test::build_valid_project(gen);
    const auto main_seq_id = project.main_sequence;
    // Lock track 1 (Audio, holds linked partner A1)
    project.sequences.at(main_seq_id).tracks[1].locked = true;

    auto ed_res = Editor::create(std::move(project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto v1_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].id;

    // V1's track (video) is unlocked, but partner A1 is on locked track
    DeleteClips cmd{main_seq_id, {v1_id}, false};

    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
    EXPECT_NE(res.error().message().find("locked"), std::string::npos);
}

TEST(DeleteClipsTest, DuplicateIdsAreDeduplicated) {
    core::UuidGenerator gen(5006ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto v1_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].id;

    DeleteClips cmd{main_seq_id, {v1_id, v1_id, v1_id}, false};

    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& v_clips = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips;
    EXPECT_EQ(v_clips.size(), 2U);
}

}  // namespace
}  // namespace nxtcut::commands
