#include <nxtcut/commands/editor.hpp>
#include <nxtcut/commands/marker_commands.hpp>
#include <nxtcut/core/color.hpp>
#include <nxtcut/core/time.hpp>
#include <nxtcut/core/uuid.hpp>
#include <nxtcut/model/ids.hpp>
#include <nxtcut/model/project.hpp>
#include <nxtcut/model/sequence.hpp>
#include <nxtcut/model/time_coords.hpp>

#include <gtest/gtest.h>

#include "test_helper.hpp"
#include <nxtcut_test/model_fixtures.hpp>

namespace nxtcut::commands {
namespace {

TEST(MarkerCommandsTest, AddMarkersOrderedByTimeAndId) {
    core::UuidGenerator gen(601ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;

    // Add marker 1 at 5 s
    AddMarker cmd1{main_seq_id, model::test::timeline_at_seconds(5), "M_5s_A",
                   core::Color{1.0f, 0.0f, 0.0f, 1.0f}};
    EXPECT_EQ(cmd1.label(), "Add Marker");
    EXPECT_EQ(cmd1.text, "M_5s_A");
    auto r1 = test::round_trip(editor, cmd1);
    ASSERT_TRUE(test::is_ok(r1));
    const auto id1 = r1->created_markers[0];

    // Add marker 2 at 2 s
    AddMarker cmd2{main_seq_id, model::test::timeline_at_seconds(2), "M_2s",
                   core::Color{0.0f, 1.0f, 0.0f, 1.0f}};
    auto r2 = test::round_trip(editor, cmd2);
    ASSERT_TRUE(test::is_ok(r2));
    const auto id2 = r2->created_markers[0];

    // Add marker 3 at 5 s
    AddMarker cmd3{main_seq_id, model::test::timeline_at_seconds(5), "M_5s_B",
                   core::Color{0.0f, 0.0f, 1.0f, 1.0f}};
    auto r3 = test::round_trip(editor, cmd3);
    ASSERT_TRUE(test::is_ok(r3));
    const auto id3 = r3->created_markers[0];

    const auto& markers = editor.snapshot()->sequences.at(main_seq_id).markers;
    ASSERT_EQ(markers.size(), 3U);

    // Vector order must be 2 s first, then the two 5 s markers ordered by id
    EXPECT_EQ(markers[0].id, id2);
    EXPECT_EQ(markers[0].time.ticks(), 2 * core::kTicksPerSecond);
    EXPECT_EQ(markers[0].label, "M_2s");

    const auto expected_first_5s = (id1 < id3) ? id1 : id3;
    const auto expected_second_5s = (id1 < id3) ? id3 : id1;
    EXPECT_EQ(markers[1].id, expected_first_5s);
    EXPECT_EQ(markers[1].time.ticks(), 5 * core::kTicksPerSecond);
    EXPECT_EQ(markers[2].id, expected_second_5s);
    EXPECT_EQ(markers[2].time.ticks(), 5 * core::kTicksPerSecond);
}

TEST(MarkerCommandsTest, AddMarkerNegativeTimeGivesInvalidArgument) {
    core::UuidGenerator gen(602ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto snap_before = editor.snapshot();
    const bool can_undo_before = editor.can_undo();

    AddMarker cmd{main_seq_id, model::TimelineTime::from_ticks(-100), "NegativeMarker",
                  core::Color{1.0f, 1.0f, 1.0f, 1.0f}};
    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
    EXPECT_EQ(editor.snapshot(), snap_before);
    EXPECT_EQ(editor.can_undo(), can_undo_before);
}

TEST(MarkerCommandsTest, RemoveMarkerSucceedsAndRestoresOnUndo) {
    core::UuidGenerator gen(603ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;

    // Add marker first
    AddMarker add_cmd{main_seq_id, model::test::timeline_at_seconds(1), "ToRem",
                      core::Color{1.0f, 1.0f, 1.0f, 1.0f}};
    auto add_rec = test::round_trip(editor, add_cmd);
    ASSERT_TRUE(test::is_ok(add_rec));
    const auto marker_id = add_rec->created_markers[0];

    // Remove it
    RemoveMarker rm_cmd{main_seq_id, marker_id};
    ASSERT_TRUE(test::is_ok(test::round_trip(editor, rm_cmd)));
    EXPECT_TRUE(editor.snapshot()->sequences.at(main_seq_id).markers.empty());
}

TEST(MarkerCommandsTest, RemoveMarkerUnknownGivesNotFound) {
    core::UuidGenerator gen(604ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto unknown_mid = model::generate_id<model::MarkerId>(gen);

    const auto snap_before = editor.snapshot();
    const bool can_undo_before = editor.can_undo();

    RemoveMarker cmd{main_seq_id, unknown_mid};
    auto res = editor.execute(cmd);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::NotFound);
    EXPECT_EQ(editor.snapshot(), snap_before);
    EXPECT_EQ(editor.can_undo(), can_undo_before);
}

}  // namespace
}  // namespace nxtcut::commands
