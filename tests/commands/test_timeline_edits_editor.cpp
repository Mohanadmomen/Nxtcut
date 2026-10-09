#include <nxtcut/commands/editor.hpp>
#include <nxtcut/commands/timeline_commands.hpp>
#include <nxtcut/core/time.hpp>
#include <nxtcut/core/uuid.hpp>
#include <nxtcut/model/clip.hpp>
#include <nxtcut/model/equality.hpp>
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

TEST(TimelineEditsEditorTest, TransactionBatchCommitsAsSingleStepAndUndoRestoresInitial) {
    core::UuidGenerator gen(1401ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto initial_snapshot = editor.snapshot();
    const auto main_seq_id = initial_snapshot->main_sequence;
    const auto v_track_id = initial_snapshot->sequences.at(main_seq_id).tracks[0].id;
    const auto image_media_id =
        std::get<model::ImageContent>(
            initial_snapshot->sequences.at(main_seq_id).tracks[0].clips[1].content)
            .media;

    auto tx_res = editor.begin_transaction("Batch Geometry Edit");
    ASSERT_TRUE(test::is_ok(tx_res));
    auto& tx = tx_res.value();

    // 1. InsertClips: 2s image at 7s
    model::Clip new_img;
    new_img.duration = duration_of_seconds(2);
    new_img.content = model::ImageContent{image_media_id};
    InsertClips insert_cmd{main_seq_id, timeline_at_seconds(7), {{v_track_id, new_img}}};
    ASSERT_TRUE(test::is_ok(tx.execute(insert_cmd)));

    // 2. SplitClip: split at 13s (inside V3, which shifted to 12..17s)
    const auto v3_id = tx.project().sequences.at(main_seq_id).tracks[0].clips[4].id;
    SplitClip split_cmd{main_seq_id, v3_id, timeline_at_seconds(13), true};
    ASSERT_TRUE(test::is_ok(tx.execute(split_cmd)));

    // 3. DeleteClips: delete V2 right half at 9..12s
    const auto v2_right_id = tx.project().sequences.at(main_seq_id).tracks[0].clips[3].id;
    DeleteClips delete_cmd{main_seq_id, {v2_right_id}, true};
    ASSERT_TRUE(test::is_ok(tx.execute(delete_cmd)));

    // Commit transaction
    auto commit_res = tx.commit();
    ASSERT_TRUE(test::is_ok(commit_res));

    EXPECT_TRUE(editor.can_undo());
    EXPECT_EQ(editor.undo_label(), "Batch Geometry Edit");

    // Undo should restore a project identical to initial_snapshot
    auto undo_res = editor.undo();
    ASSERT_TRUE(test::is_ok(undo_res));
    EXPECT_TRUE(model::identical(*editor.snapshot(), *initial_snapshot));
}

TEST(TimelineEditsEditorTest, FailedCommandInsideTransactionRollbackLeavesProjectUnchanged) {
    core::UuidGenerator gen(1402ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto initial_snapshot = editor.snapshot();
    const auto main_seq_id = initial_snapshot->main_sequence;
    const auto v_track_id = initial_snapshot->sequences.at(main_seq_id).tracks[0].id;
    const auto image_media_id =
        std::get<model::ImageContent>(
            initial_snapshot->sequences.at(main_seq_id).tracks[0].clips[1].content)
            .media;

    auto tx_res = editor.begin_transaction("Failing Tx");
    ASSERT_TRUE(test::is_ok(tx_res));
    auto& tx = tx_res.value();

    // Valid command
    model::Clip new_img;
    new_img.duration = duration_of_seconds(2);
    new_img.content = model::ImageContent{image_media_id};
    InsertClips insert_cmd{main_seq_id, timeline_at_seconds(7), {{v_track_id, new_img}}};
    ASSERT_TRUE(test::is_ok(tx.execute(insert_cmd)));

    // Invalid command: overlap at 7s
    AddClips bad_cmd{main_seq_id, timeline_at_seconds(7), {{v_track_id, new_img}}};
    auto bad_res = tx.execute(bad_cmd);
    EXPECT_FALSE(bad_res.has_value());

    // Explicit rollback
    tx.rollback();

    // Snapshot is completely unchanged
    EXPECT_TRUE(model::identical(*editor.snapshot(), *initial_snapshot));
    EXPECT_FALSE(editor.can_undo());
}

TEST(TimelineEditsEditorTest, EveryCommandProducesNonEmptyUndoLabel) {
    core::UuidGenerator gen(1403ULL);

    const auto check_label = [](const auto& cmd, std::string_view expected) {
        EXPECT_FALSE(cmd.label().empty());
        EXPECT_EQ(cmd.label(), expected);
    };

    model::SequenceId s{};
    model::TrackId t{};
    model::ClipId c{};
    model::Clip cl{};

    check_label(AddClips{s, {}, {{t, cl}}}, "Add Clips");
    check_label(InsertClips{s, {}, {{t, cl}}}, "Insert Clips");
    check_label(OverwriteClips{s, {}, {{t, cl}}}, "Overwrite Clips");
    check_label(MoveClips{s, {{c, {}, t}}}, "Move Clips");
    check_label(DeleteClips{s, {c}}, "Delete Clips");
    check_label(SplitClip{s, c, {}}, "Split Clip");
    check_label(TrimClip{s, c, TrimEdge::Tail, {}}, "Trim Clip");
    check_label(RippleDeleteClips{s, {c}}, "Ripple Delete Clips");
    check_label(CloseGap{s, t, {}}, "Close Gap");
    check_label(JoinClips{s, {{c, c}}}, "Join Clips");
    check_label(LinkClips{s, {c, c}}, "Link Clips");
    check_label(UnlinkClips{s, {c}}, "Unlink Clips");
}

TEST(TimelineEditsEditorTest, DeterminismAcrossIdenticallySeededEditors) {
    // Editor 1
    core::UuidGenerator gen1(1404ULL);
    auto ed1_res = Editor::create(model::test::build_valid_project(gen1), gen1);
    ASSERT_TRUE(test::is_ok(ed1_res));
    Editor& editor1 = *ed1_res.value();

    // Editor 2
    core::UuidGenerator gen2(1404ULL);
    auto ed2_res = Editor::create(model::test::build_valid_project(gen2), gen2);
    ASSERT_TRUE(test::is_ok(ed2_res));
    Editor& editor2 = *ed2_res.value();

    const auto main_seq_id1 = editor1.snapshot()->main_sequence;
    const auto v_track_id1 = editor1.snapshot()->sequences.at(main_seq_id1).tracks[0].id;
    const auto image_media_id1 =
        std::get<model::ImageContent>(
            editor1.snapshot()->sequences.at(main_seq_id1).tracks[0].clips[1].content)
            .media;

    model::Clip img1;
    img1.duration = duration_of_seconds(2);
    img1.content = model::ImageContent{image_media_id1};

    InsertClips cmd1{main_seq_id1, timeline_at_seconds(7), {{v_track_id1, img1}}};
    ASSERT_TRUE(test::is_ok(editor1.execute(cmd1)));

    const auto main_seq_id2 = editor2.snapshot()->main_sequence;
    const auto v_track_id2 = editor2.snapshot()->sequences.at(main_seq_id2).tracks[0].id;
    const auto image_media_id2 =
        std::get<model::ImageContent>(
            editor2.snapshot()->sequences.at(main_seq_id2).tracks[0].clips[1].content)
            .media;

    model::Clip img2;
    img2.duration = duration_of_seconds(2);
    img2.content = model::ImageContent{image_media_id2};

    InsertClips cmd2{main_seq_id2, timeline_at_seconds(7), {{v_track_id2, img2}}};
    ASSERT_TRUE(test::is_ok(editor2.execute(cmd2)));

    EXPECT_TRUE(model::identical(*editor1.snapshot(), *editor2.snapshot()));
}

}  // namespace
}  // namespace nxtcut::commands
