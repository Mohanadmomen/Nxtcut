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

TEST(UnlinkClipsTest, UnlinkingV1ClearsBothV1andA1) {
    core::UuidGenerator gen(1301ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto v1_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].id;

    UnlinkClips cmd{main_seq_id, {v1_id}, false};

    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& v_clips = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips;
    const auto& a_clips = editor.snapshot()->sequences.at(main_seq_id).tracks[1].clips;

    EXPECT_FALSE(v_clips[0].link_id.has_value());
    EXPECT_FALSE(a_clips[0].link_id.has_value());
}

TEST(UnlinkClipsTest, IgnoreLinksClearsNamedClipAndRepairClearsPartner) {
    core::UuidGenerator gen(1302ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto v1_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].id;

    UnlinkClips cmd{main_seq_id, {v1_id}, true};

    auto rt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(rt_res));

    const auto& v_clips = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips;
    const auto& a_clips = editor.snapshot()->sequences.at(main_seq_id).tracks[1].clips;

    EXPECT_FALSE(v_clips[0].link_id.has_value());
    EXPECT_FALSE(a_clips[0].link_id.has_value());
}

TEST(UnlinkClipsTest, UnlinkingUnlinkedClipReturnsEmptyChangeSet) {
    core::UuidGenerator gen(1303ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto v2_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[1].id;

    // V2 has no link
    EXPECT_FALSE(editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[1].link_id.has_value());

    UnlinkClips cmd{main_seq_id, {v2_id}, false};

    auto build_res = cmd.build(*editor.snapshot(), gen);
    ASSERT_TRUE(test::is_ok(build_res));
    EXPECT_TRUE(build_res.value().empty());

    auto exec_res = editor.execute(cmd);
    ASSERT_TRUE(test::is_ok(exec_res));
    EXPECT_FALSE(editor.can_undo());
}

TEST(UnlinkClipsTest, UnknownClipGivesNotFound) {
    core::UuidGenerator gen(1304ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto fake_clip_id = model::generate_id<model::ClipId>(gen);

    UnlinkClips cmd{main_seq_id, {fake_clip_id}, false};

    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::NotFound);
}

TEST(UnlinkClipsTest, LockedTrackFailsWithLockedMessage) {
    core::UuidGenerator gen(1305ULL);
    auto project = model::test::build_valid_project(gen);
    const auto main_seq_id = project.main_sequence;
    project.sequences.at(main_seq_id).tracks[0].locked = true;

    auto ed_res = Editor::create(std::move(project), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto v1_id = editor.snapshot()->sequences.at(main_seq_id).tracks[0].clips[0].id;

    UnlinkClips cmd{main_seq_id, {v1_id}, false};

    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
    EXPECT_NE(res.error().message().find("locked"), std::string::npos);
}

}  // namespace
}  // namespace nxtcut::commands
