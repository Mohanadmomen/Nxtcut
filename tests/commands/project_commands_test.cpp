#include <nxtcut/commands/editor.hpp>
#include <nxtcut/commands/project_commands.hpp>
#include <nxtcut/core/uuid.hpp>
#include <nxtcut/model/ids.hpp>
#include <nxtcut/model/project.hpp>

#include <gtest/gtest.h>

#include <string>

#include "test_helper.hpp"
#include <nxtcut_test/model_fixtures.hpp>

namespace nxtcut::commands {
namespace {

TEST(ProjectCommandsTest, SetProjectPropertiesRenameAndChangeMainSequence) {
    core::UuidGenerator gen(801ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    // 1. Rename
    SetProjectProperties rename_cmd{"NewProjectName", std::nullopt};
    ASSERT_TRUE(test::is_ok(test::round_trip(editor, rename_cmd)));
    EXPECT_EQ(editor.snapshot()->name, "NewProjectName");

    // 2. Set main sequence to the other fixture sequence
    model::SequenceId other_seq_id{};
    for (const auto& [sid, _] : editor.snapshot()->sequences) {
        if (sid != editor.snapshot()->main_sequence) {
            other_seq_id = sid;
            break;
        }
    }
    ASSERT_FALSE(other_seq_id.is_nil());

    SetProjectProperties seq_cmd{std::nullopt, other_seq_id};
    ASSERT_TRUE(test::is_ok(test::round_trip(editor, seq_cmd)));
    EXPECT_EQ(editor.snapshot()->main_sequence, other_seq_id);
}

TEST(ProjectCommandsTest, SetProjectPropertiesUnknownMainSequenceNotFound) {
    core::UuidGenerator gen(802ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto unknown_seq = model::generate_id<model::SequenceId>(gen);
    const auto snap_before = editor.snapshot();
    const bool can_undo_before = editor.can_undo();

    SetProjectProperties cmd{std::nullopt, unknown_seq};
    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::NotFound);
    EXPECT_EQ(editor.snapshot(), snap_before);
    EXPECT_EQ(editor.can_undo(), can_undo_before);
}

TEST(ProjectCommandsTest, SetProjectPropertiesAllEmptyInvalidArgument) {
    core::UuidGenerator gen(803ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto snap_before = editor.snapshot();
    const bool can_undo_before = editor.can_undo();

    SetProjectProperties cmd{std::nullopt, std::nullopt};
    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
    EXPECT_EQ(editor.snapshot(), snap_before);
    EXPECT_EQ(editor.can_undo(), can_undo_before);
}

}  // namespace
}  // namespace nxtcut::commands
