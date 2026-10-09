#include <nxtcut/commands/editor.hpp>
#include <nxtcut/commands/sequence_commands.hpp>
#include <nxtcut/core/color.hpp>
#include <nxtcut/core/frame_rate.hpp>
#include <nxtcut/core/geometry.hpp>
#include <nxtcut/core/uuid.hpp>
#include <nxtcut/model/ids.hpp>
#include <nxtcut/model/project.hpp>
#include <nxtcut/model/sequence.hpp>

#include <gtest/gtest.h>

#include "test_helper.hpp"
#include <nxtcut_test/model_fixtures.hpp>

namespace nxtcut::commands {
namespace {

TEST(SequenceCommandsTest, CreateSequenceSuccessAndReceipt) {
    core::UuidGenerator gen(701ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    CreateSequence cmd{"ExtraSequence", core::frame_rates::k60,
                       core::Size<std::int32_t>{3840, 2160}, core::sample_rates::k96000,
                       core::Color{0.1f, 0.1f, 0.1f, 1.0f}};

    auto receipt_res = test::round_trip(editor, cmd);
    ASSERT_TRUE(test::is_ok(receipt_res));

    ASSERT_EQ(receipt_res->created_sequences.size(), 1U);
    const auto new_seq_id = receipt_res->created_sequences[0];

    const auto* found = model::find_sequence(*editor.snapshot(), new_seq_id);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->name, "ExtraSequence");
    EXPECT_TRUE(found->tracks.empty());
}

TEST(SequenceCommandsTest, CreateSequenceCanvasWidthZeroFails) {
    core::UuidGenerator gen(702ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto snap_before = editor.snapshot();
    const bool can_undo_before = editor.can_undo();

    CreateSequence cmd{"BadCanvas", core::frame_rates::k30, core::Size<std::int32_t>{0, 1080},
                       core::sample_rates::k48000, core::Color{0.0f, 0.0f, 0.0f, 1.0f}};

    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
    EXPECT_EQ(editor.snapshot(), snap_before);
    EXPECT_EQ(editor.can_undo(), can_undo_before);
}

TEST(SequenceCommandsTest, RemoveSequenceUnusedSucceeds) {
    core::UuidGenerator gen(703ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    // Create an unused sequence first
    CreateSequence add_cmd{"ToDrop", core::frame_rates::k30, core::Size<std::int32_t>{1920, 1080},
                           core::sample_rates::k48000, core::Color{0.0f, 0.0f, 0.0f, 1.0f}};
    auto add_rec = test::round_trip(editor, add_cmd);
    ASSERT_TRUE(test::is_ok(add_rec));
    const auto seq_to_drop = add_rec->created_sequences[0];

    RemoveSequence rm_cmd{seq_to_drop};
    ASSERT_TRUE(test::is_ok(test::round_trip(editor, rm_cmd)));
    EXPECT_EQ(model::find_sequence(*editor.snapshot(), seq_to_drop), nullptr);
}

TEST(SequenceCommandsTest, RemoveSequenceMainSequenceFails) {
    core::UuidGenerator gen(704ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;

    const auto snap_before = editor.snapshot();
    const bool can_undo_before = editor.can_undo();

    RemoveSequence cmd{main_seq_id};
    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
    EXPECT_EQ(editor.snapshot(), snap_before);
    EXPECT_EQ(editor.can_undo(), can_undo_before);
}

TEST(SequenceCommandsTest, RemoveSequenceInUseByCompoundClipFails) {
    core::UuidGenerator gen(705ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    // In fixture, the non-main sequence is referenced by MainCompound clip
    model::SequenceId non_main_id{};
    for (const auto& [sid, _] : editor.snapshot()->sequences) {
        if (sid != editor.snapshot()->main_sequence) {
            non_main_id = sid;
            break;
        }
    }
    ASSERT_FALSE(non_main_id.is_nil());

    const auto snap_before = editor.snapshot();
    const bool can_undo_before = editor.can_undo();

    RemoveSequence cmd{non_main_id};
    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
    EXPECT_NE(res.error().message().find("in use"), std::string_view::npos);
    EXPECT_EQ(editor.snapshot(), snap_before);
    EXPECT_EQ(editor.can_undo(), can_undo_before);
}

TEST(SequenceCommandsTest, RemoveSequenceUnknownNotFound) {
    core::UuidGenerator gen(706ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto unknown_seq = model::generate_id<model::SequenceId>(gen);
    const auto snap_before = editor.snapshot();
    const bool can_undo_before = editor.can_undo();

    RemoveSequence cmd{unknown_seq};
    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::NotFound);
    EXPECT_EQ(editor.snapshot(), snap_before);
    EXPECT_EQ(editor.can_undo(), can_undo_before);
}

TEST(SequenceCommandsTest, SetSequenceSettingsRenameAndCanvas) {
    core::UuidGenerator gen(707ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;

    // 1. Rename
    SetSequenceSettings rename_cmd{main_seq_id,  "RenamedSeq", std::nullopt,
                                   std::nullopt, std::nullopt, std::nullopt};
    ASSERT_TRUE(test::is_ok(test::round_trip(editor, rename_cmd)));
    EXPECT_EQ(editor.snapshot()->sequences.at(main_seq_id).name, "RenamedSeq");

    // 2. Change canvas to 1280x720
    SetSequenceSettings canvas_cmd{main_seq_id,  std::nullopt,
                                   std::nullopt, core::Size<std::int32_t>{1280, 720},
                                   std::nullopt, std::nullopt};
    ASSERT_TRUE(test::is_ok(test::round_trip(editor, canvas_cmd)));
    EXPECT_EQ(editor.snapshot()->sequences.at(main_seq_id).canvas.width, 1280);
    EXPECT_EQ(editor.snapshot()->sequences.at(main_seq_id).canvas.height, 720);
}

TEST(SequenceCommandsTest, SetSequenceSettingsCanvasHeightZeroFails) {
    core::UuidGenerator gen(708ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto snap_before = editor.snapshot();
    const bool can_undo_before = editor.can_undo();

    SetSequenceSettings cmd{main_seq_id,  std::nullopt,
                            std::nullopt, core::Size<std::int32_t>{1920, 0},
                            std::nullopt, std::nullopt};
    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
    EXPECT_EQ(editor.snapshot(), snap_before);
    EXPECT_EQ(editor.can_undo(), can_undo_before);
}

TEST(SequenceCommandsTest, SetSequenceSettingsAllEmptyInvalidArgument) {
    core::UuidGenerator gen(709ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto snap_before = editor.snapshot();
    const bool can_undo_before = editor.can_undo();

    SetSequenceSettings cmd{main_seq_id,  std::nullopt, std::nullopt,
                            std::nullopt, std::nullopt, std::nullopt};
    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
    EXPECT_EQ(editor.snapshot(), snap_before);
    EXPECT_EQ(editor.can_undo(), can_undo_before);
}

TEST(SequenceCommandsTest, SetSequenceSettingsSameValuesNoHistoryEntry) {
    core::UuidGenerator gen(710ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto& seq = editor.snapshot()->sequences.at(main_seq_id);

    const auto snap_before = editor.snapshot();
    const bool can_undo_before = editor.can_undo();

    SetSequenceSettings cmd{main_seq_id, seq.name,        seq.frame_rate,
                            seq.canvas,  seq.sample_rate, seq.background};
    auto res = editor.execute(cmd);
    ASSERT_TRUE(test::is_ok(res));
    EXPECT_EQ(editor.snapshot(), snap_before);
    EXPECT_EQ(editor.can_undo(), can_undo_before);
}

}  // namespace
}  // namespace nxtcut::commands
