#include <nxtcut/commands/editor.hpp>
#include <nxtcut/commands/timeline_commands.hpp>
#include <nxtcut/core/time.hpp>
#include <nxtcut/core/uuid.hpp>
#include <nxtcut/model/clip.hpp>
#include <nxtcut/model/ids.hpp>
#include <nxtcut/model/project.hpp>
#include <nxtcut/model/time_coords.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <nxtcut_test/assertions.hpp>
#include <nxtcut_test/model_fixtures.hpp>
#include "test_helper.hpp"

namespace nxtcut::commands {
namespace {

TEST(LinkClipsTest, LinkTwoUnlinkedClipsGivesOneFreshLinkId) {
    core::UuidGenerator gen(1201ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto v2_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[1].id;
    const auto v3_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[2].id;

    EXPECT_FALSE(editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[1].link_id.has_value());
    EXPECT_FALSE(editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[2].link_id.has_value());

    LinkClips cmd{main_seq_id, {v2_id, v3_id}};

    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& v_clips = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips;
    ASSERT_TRUE(v_clips[1].link_id.has_value());
    ASSERT_TRUE(v_clips[2].link_id.has_value());
    EXPECT_EQ(v_clips[1].link_id, v_clips[2].link_id);
}

TEST(LinkClipsTest, AlreadyLinkedClipGivesInvalidArgument) {
    core::UuidGenerator gen(1202ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto v1_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].id;
    const auto v2_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[1].id;

    // V1 is already linked with A1
    LinkClips cmd{main_seq_id, {v1_id, v2_id}};

    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
    EXPECT_NE(res.error().message().find("already linked"), std::string::npos);
}

TEST(LinkClipsTest, OneClipGivesInvalidArgument) {
    core::UuidGenerator gen(1203ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto v2_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[1].id;

    LinkClips cmd{main_seq_id, {v2_id}};

    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
}

TEST(LinkClipsTest, ClipsInDifferentSequencesGivesNotFound) {
    core::UuidGenerator gen(1204ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto v2_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[1].id;

    // Find sub_clip_id in SubSequence
    model::ClipId sub_clip_id;
    for (const auto& [s_id, seq] : editor.snapshot()->sequences) {
        if (s_id != main_seq_id) {
            sub_clip_id = seq.tracks[0].clips[0].id;
            break;
        }
    }

    // Attempting to link clips from different sequences in main_sequence
    // sub_clip_id is not in main_seq_id, so find_clip returns NotFound
    LinkClips cmd{main_seq_id, {v2_id, sub_clip_id}};

    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_TRUE(res.error().code() == core::ErrorCode::NotFound ||
                res.error().code() == core::ErrorCode::InvalidArgument);
}

TEST(LinkClipsTest, LockedTrackFailsWithLockedMessage) {
    core::UuidGenerator gen(1205ULL);
    auto project = model::test::build_valid_project(gen);
    const auto main_seq_id = project.main_sequence;
    project.sequences.at(main_seq_id).tracks[0].locked = true;

    auto ed_res = Editor::create(std::move(project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto v2_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[1].id;
    const auto v3_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[2].id;

    LinkClips cmd{main_seq_id, {v2_id, v3_id}};

    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
    EXPECT_NE(res.error().message().find("locked"), std::string::npos);
}

}  // namespace
}  // namespace nxtcut::commands
