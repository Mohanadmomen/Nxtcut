#include <nxtcut/commands/editor.hpp>
#include <nxtcut/commands/marker_commands.hpp>
#include <nxtcut/commands/media_commands.hpp>
#include <nxtcut/commands/track_commands.hpp>
#include <nxtcut/core/color.hpp>
#include <nxtcut/core/time.hpp>
#include <nxtcut/core/uuid.hpp>
#include <nxtcut/model/clip.hpp>
#include <nxtcut/model/equality.hpp>
#include <nxtcut/model/ids.hpp>
#include <nxtcut/model/project.hpp>
#include <nxtcut/model/track.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "test_helper.hpp"
#include <nxtcut_test/model_fixtures.hpp>

namespace nxtcut::commands {
namespace {

struct InvalidClipDurationCommand {
    model::SequenceId sequence;
    model::TrackId track;
    model::ClipId clip;

    [[nodiscard]] std::string label() const { return "MakeClipInvalid"; }
    [[nodiscard]] core::Result<ChangeSet> build(const model::Project& project,
                                                core::UuidGenerator& ids) const {
        static_cast<void>(ids);
        const auto* found = model::find_clip(project.sequences.at(sequence), clip);
        if (found == nullptr) {
            return core::make_error(core::ErrorCode::NotFound, "clip not found");
        }
        model::Clip invalid_clip = *found;
        invalid_clip.duration = core::Duration::zero();
        ChangeSet cs;
        cs.label = label();
        cs.changes.push_back(ClipChange{sequence, track, clip, *found, invalid_clip});
        return cs;
    }
};

TEST(EditorTest, CreateMaxHistoryStepsZeroFails) {
    core::UuidGenerator gen(901ULL);
    EditorOptions options;
    options.max_history_steps = 0;

    auto res = Editor::create(model::test::build_valid_project(gen), gen, options);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
}

TEST(EditorTest, CreateInvalidInitialProjectFails) {
    core::UuidGenerator gen(902ULL);
    model::Project p = model::test::build_valid_project(gen);
    p.sequences.at(p.main_sequence).tracks[0].clips[0].duration = core::Duration::zero();

    auto res = Editor::create(std::move(p), gen);
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), core::ErrorCode::InvalidArgument);
}

TEST(EditorTest, CreateWithUnsortedTrackNormalizesIt) {
    core::UuidGenerator gen(903ULL);
    model::Project p = model::test::build_valid_project(gen);

    // Make an unsorted track: append clip at 30 s before clip at 20 s
    auto& track = p.sequences.at(p.main_sequence).tracks[0];
    model::Clip c1 = track.clips[0];
    c1.id = model::generate_id<model::ClipId>(gen);
    c1.start = model::test::timeline_at_seconds(30);
    c1.link_id = std::nullopt;

    model::Clip c2 = track.clips[0];
    c2.id = model::generate_id<model::ClipId>(gen);
    c2.start = model::test::timeline_at_seconds(20);
    c2.link_id = std::nullopt;

    track.clips.push_back(c1);
    track.clips.push_back(c2);

    auto res = Editor::create(std::move(p), gen);
    ASSERT_TRUE(test::is_ok(res));

    const auto& norm_clips = res.value()
                                 ->snapshot()
                                 ->sequences.at(res.value()->snapshot()->main_sequence)
                                 .tracks[0]
                                 .clips;
    ASSERT_GE(norm_clips.size(), 2U);
    EXPECT_EQ(norm_clips[norm_clips.size() - 2].id, c2.id);
    EXPECT_EQ(norm_clips[norm_clips.size() - 1].id, c1.id);

    for (std::size_t i = 1; i < norm_clips.size(); ++i) {
        EXPECT_LE(norm_clips[i - 1].start.ticks(), norm_clips[i].start.ticks());
    }
}

TEST(EditorTest, TransactionWithThreeCommandsIsOneUndoStep) {
    core::UuidGenerator gen(904ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto initial_snapshot = editor.snapshot();

    auto tx_res = editor.begin_transaction("BatchOfThree");
    ASSERT_TRUE(test::is_ok(tx_res));
    auto& tx = *tx_res;

    // Command 1: Add track
    AddTrack cmd1{main_seq_id, model::TrackKind::Video, "BatchTrack1", std::nullopt};
    ASSERT_TRUE(test::is_ok(tx.execute(cmd1)));

    // Command 2: Add track
    AddTrack cmd2{main_seq_id, model::TrackKind::Audio, "BatchTrack2", std::nullopt};
    ASSERT_TRUE(test::is_ok(tx.execute(cmd2)));

    // Command 3: Rename first track
    const auto t0_id = tx.project().sequences.at(main_seq_id).tracks[0].id;
    SetTrackProperties cmd3{main_seq_id, t0_id, "RenamedInBatch", std::nullopt, std::nullopt};
    ASSERT_TRUE(test::is_ok(tx.execute(cmd3)));

    auto commit_res = tx.commit();
    ASSERT_TRUE(test::is_ok(commit_res));
    EXPECT_EQ(commit_res->created_tracks.size(), 2U);

    const auto after_snapshot = editor.snapshot();
    EXPECT_NE(after_snapshot, initial_snapshot);
    EXPECT_TRUE(editor.can_undo());

    // One undo reverts all three
    auto undo_status = editor.undo();
    ASSERT_TRUE(test::is_ok(undo_status));
    EXPECT_FALSE(editor.can_undo());
    EXPECT_TRUE(model::identical(*editor.snapshot(), *initial_snapshot));

    // Redo reapplies all three
    auto redo_status = editor.redo();
    ASSERT_TRUE(test::is_ok(redo_status));
    EXPECT_TRUE(model::identical(*editor.snapshot(), *after_snapshot));
}

TEST(EditorTest, DestroyingUncommittedTransactionOrRollbackLeavesStateUntouched) {
    core::UuidGenerator gen(905ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto initial_snapshot = editor.snapshot();
    const auto main_seq_id = initial_snapshot->main_sequence;

    // 1. Destructor rollback
    {
        auto tx_res = editor.begin_transaction("DroppedTx");
        ASSERT_TRUE(test::is_ok(tx_res));
        AddTrack cmd{main_seq_id, model::TrackKind::Video, "Uncommitted", std::nullopt};
        ASSERT_TRUE(test::is_ok(tx_res->execute(cmd)));
        // Destructor runs here without commit
    }

    EXPECT_EQ(editor.snapshot(), initial_snapshot);
    EXPECT_FALSE(editor.can_undo());

    // 2. Explicit rollback()
    {
        auto tx_res = editor.begin_transaction("ExplicitRollbackTx");
        ASSERT_TRUE(test::is_ok(tx_res));
        AddTrack cmd{main_seq_id, model::TrackKind::Video, "Uncommitted2", std::nullopt};
        ASSERT_TRUE(test::is_ok(tx_res->execute(cmd)));
        tx_res->rollback();
    }

    EXPECT_EQ(editor.snapshot(), initial_snapshot);
    EXPECT_FALSE(editor.can_undo());
}

TEST(EditorTest, SecondBeginTransactionFails) {
    core::UuidGenerator gen(906ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    auto tx1 = editor.begin_transaction("FirstTx");
    ASSERT_TRUE(test::is_ok(tx1));

    auto tx2 = editor.begin_transaction("SecondTx");
    ASSERT_FALSE(tx2.has_value());
    EXPECT_EQ(tx2.error().code(), core::ErrorCode::InvalidArgument);
    EXPECT_NE(tx2.error().message().find("transaction open"), std::string_view::npos);
}

TEST(EditorTest, OperationsBlockedWhileTransactionIsOpen) {
    core::UuidGenerator gen(907ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;

    auto tx = editor.begin_transaction("OpenTx");
    ASSERT_TRUE(test::is_ok(tx));

    AddTrack cmd{main_seq_id, model::TrackKind::Video, "T", std::nullopt};
    auto exec_res = editor.execute(cmd);
    ASSERT_FALSE(exec_res.has_value());
    EXPECT_EQ(exec_res.error().code(), core::ErrorCode::InvalidArgument);

    auto undo_res = editor.undo();
    ASSERT_FALSE(undo_res.has_value());
    EXPECT_EQ(undo_res.error().code(), core::ErrorCode::InvalidArgument);

    auto redo_res = editor.redo();
    ASSERT_FALSE(redo_res.has_value());
    EXPECT_EQ(redo_res.error().code(), core::ErrorCode::InvalidArgument);
}

TEST(EditorTest, TransactionFinalStateInvalidRejectedAtCommit) {
    core::UuidGenerator gen(908ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto initial_snapshot = editor.snapshot();
    const auto main_seq_id = initial_snapshot->main_sequence;
    const auto& track = initial_snapshot->sequences.at(main_seq_id).tracks[0];
    const auto clip_id = track.clips[0].id;

    auto tx_res = editor.begin_transaction("InvalidBatch");
    ASSERT_TRUE(test::is_ok(tx_res));
    auto& tx = *tx_res;

    // Command 1: valid track rename
    SetTrackProperties cmd1{main_seq_id, track.id, "Renamed", std::nullopt, std::nullopt};
    ASSERT_TRUE(test::is_ok(tx.execute(cmd1)));

    // Command 2: make clip invalid by setting duration to 0
    InvalidClipDurationCommand cmd2{main_seq_id, track.id, clip_id};
    ASSERT_TRUE(test::is_ok(tx.execute(cmd2)));

    auto commit_res = tx.commit();
    ASSERT_FALSE(commit_res.has_value());
    EXPECT_EQ(commit_res.error().code(), core::ErrorCode::InvalidArgument);
    EXPECT_NE(commit_res.error().message().find("edit rejected"), std::string_view::npos);

    // Snapshot and history remain untouched
    EXPECT_EQ(editor.snapshot(), initial_snapshot);
    EXPECT_FALSE(editor.can_undo());
}

TEST(EditorTest, TransactionValidationDisabledAcceptsCommit) {
    core::UuidGenerator gen(909ULL);
    EditorOptions options;
    options.validate_on_commit = false;

    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen, options);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto& track = editor.snapshot()->sequences.at(main_seq_id).tracks[0];
    const auto clip_id = track.clips[0].id;

    auto tx_res = editor.begin_transaction("InvalidBatchAllowed");
    ASSERT_TRUE(test::is_ok(tx_res));
    auto& tx = *tx_res;

    InvalidClipDurationCommand cmd{main_seq_id, track.id, clip_id};
    ASSERT_TRUE(test::is_ok(tx.execute(cmd)));

    auto commit_res = tx.commit();
    ASSERT_TRUE(test::is_ok(commit_res));
    EXPECT_TRUE(editor.can_undo());
}

TEST(EditorTest, PreCommitSnapshotRemainsUnmodifiedAfterCommit) {
    core::UuidGenerator gen(910ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto pre_commit_snapshot = editor.snapshot();
    const model::Project pre_commit_copy = *pre_commit_snapshot;

    const auto main_seq_id = pre_commit_snapshot->main_sequence;
    AddTrack cmd{main_seq_id, model::TrackKind::Video, "NewTrack", std::nullopt};
    ASSERT_TRUE(test::is_ok(editor.execute(cmd)));

    const auto new_snapshot = editor.snapshot();
    EXPECT_NE(new_snapshot, pre_commit_snapshot);
    EXPECT_TRUE(model::identical(*pre_commit_snapshot, pre_commit_copy));
    EXPECT_FALSE(model::identical(*new_snapshot, *pre_commit_snapshot));
}

TEST(EditorTest, SnapshotPointerDiffersAfterUndoAndIsIdenticalToEarlier) {
    core::UuidGenerator gen(911ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto snap0 = editor.snapshot();
    const auto main_seq_id = snap0->main_sequence;

    AddTrack cmd{main_seq_id, model::TrackKind::Video, "T", std::nullopt};
    ASSERT_TRUE(test::is_ok(editor.execute(cmd)));
    const auto snap1 = editor.snapshot();

    ASSERT_TRUE(test::is_ok(editor.undo()));
    const auto snap_after_undo = editor.snapshot();

    EXPECT_NE(snap_after_undo, snap1);
    EXPECT_NE(snap_after_undo, snap0);
    EXPECT_TRUE(model::identical(*snap_after_undo, *snap0));
}

TEST(EditorTest, HistoryLimitCapacityAndClearRedoOnNewCommand) {
    core::UuidGenerator gen(912ULL);
    EditorOptions options;
    options.max_history_steps = 2;

    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen, options);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;

    AddTrack cmd1{main_seq_id, model::TrackKind::Video, "T1", std::nullopt};
    AddTrack cmd2{main_seq_id, model::TrackKind::Video, "T2", std::nullopt};
    AddTrack cmd3{main_seq_id, model::TrackKind::Video, "T3", std::nullopt};

    ASSERT_TRUE(test::is_ok(editor.execute(cmd1)));
    ASSERT_TRUE(test::is_ok(editor.execute(cmd2)));
    ASSERT_TRUE(test::is_ok(editor.execute(cmd3)));

    // With limit 2, 2 undos succeed and 3rd gives NotFound
    EXPECT_TRUE(test::is_ok(editor.undo()));
    EXPECT_TRUE(test::is_ok(editor.undo()));
    auto third_undo = editor.undo();
    ASSERT_FALSE(third_undo.has_value());
    EXPECT_EQ(third_undo.error().code(), core::ErrorCode::NotFound);

    // Now redo is available; a new command clears redo
    EXPECT_TRUE(editor.can_redo());
    AddTrack cmd4{main_seq_id, model::TrackKind::Video, "T4", std::nullopt};
    ASSERT_TRUE(test::is_ok(editor.execute(cmd4)));
    EXPECT_FALSE(editor.can_redo());
}

TEST(EditorTest, UndoAndRedoLabelsMatchCommand) {
    core::UuidGenerator gen(913ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    EXPECT_FALSE(editor.undo_label().has_value());
    EXPECT_FALSE(editor.redo_label().has_value());

    const auto main_seq_id = editor.snapshot()->main_sequence;
    AddTrack cmd{main_seq_id, model::TrackKind::Video, "T", std::nullopt};
    ASSERT_TRUE(test::is_ok(editor.execute(cmd)));

    EXPECT_EQ(editor.undo_label(), "Add Track");
    EXPECT_FALSE(editor.redo_label().has_value());

    ASSERT_TRUE(test::is_ok(editor.undo()));
    EXPECT_FALSE(editor.undo_label().has_value());
    EXPECT_EQ(editor.redo_label(), "Add Track");
}

class TestListener : public EditListener {
public:
    void on_edit(const EditEvent& event) override {
        events.push_back({event.kind, event.project != nullptr});
    }

    struct EventRecord {
        EditEventKind kind;
        bool has_project;
    };
    std::vector<EventRecord> events;
};

class OrderListener : public EditListener {
public:
    explicit OrderListener(int id, std::vector<int>& log) : id_(id), log_(log) {}
    void on_edit(const EditEvent& /*event*/) override { log_.push_back(id_); }

private:
    int id_;
    std::vector<int>& log_;
};

class ReentrantListener : public EditListener {
public:
    explicit ReentrantListener(Editor& editor, model::SequenceId seq)
        : editor_(editor), seq_(seq) {}

    void on_edit(const EditEvent& /*event*/) override {
        if (!attempted_) {
            attempted_ = true;
            AddTrack inner_cmd{seq_, model::TrackKind::Video, "Inner", std::nullopt};
            auto res = editor_.execute(inner_cmd);
            ASSERT_FALSE(res.has_value());
            reentrant_code = res.error().code();
        }
    }

    core::ErrorCode reentrant_code{core::ErrorCode::Internal};
    bool attempted_{false};

private:
    Editor& editor_;
    model::SequenceId seq_;
};

TEST(EditorTest, ListenersDeliveryAndReentrancyProtection) {
    core::UuidGenerator gen(914ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;

    auto listener = std::make_shared<TestListener>();
    editor.add_listener(listener);

    // 1. Commit event
    AddTrack cmd{main_seq_id, model::TrackKind::Video, "Track", std::nullopt};
    ASSERT_TRUE(test::is_ok(editor.execute(cmd)));
    ASSERT_EQ(listener->events.size(), 1U);
    EXPECT_EQ(listener->events[0].kind, EditEventKind::Commit);
    EXPECT_TRUE(listener->events[0].has_project);

    // 2. Undo event
    ASSERT_TRUE(test::is_ok(editor.undo()));
    ASSERT_EQ(listener->events.size(), 2U);
    EXPECT_EQ(listener->events[1].kind, EditEventKind::Undo);
    EXPECT_TRUE(listener->events[1].has_project);

    // 3. Redo event
    ASSERT_TRUE(test::is_ok(editor.redo()));
    ASSERT_EQ(listener->events.size(), 3U);
    EXPECT_EQ(listener->events[2].kind, EditEventKind::Redo);
    EXPECT_TRUE(listener->events[2].has_project);

    // 4. Failed build, empty changes, rolled-back tx do NOT call listener
    // Empty ChangeSet (same values)
    const auto& trk = editor.snapshot()->sequences.at(main_seq_id).tracks[0];
    SetTrackProperties no_op{main_seq_id, trk.id, trk.name, trk.enabled, trk.locked};
    ASSERT_TRUE(test::is_ok(editor.execute(no_op)));
    EXPECT_EQ(listener->events.size(), 3U);

    // Rolled-back transaction
    {
        auto tx = editor.begin_transaction("Abort");
        ASSERT_TRUE(test::is_ok(tx));
        AddTrack t_cmd{main_seq_id, model::TrackKind::Video, "T", std::nullopt};
        ASSERT_TRUE(test::is_ok(tx->execute(t_cmd)));
        tx->rollback();
    }
    EXPECT_EQ(listener->events.size(), 3U);

    // 5. Two listeners in registration order
    std::vector<int> call_order;
    auto o1 = std::make_shared<OrderListener>(1, call_order);
    auto o2 = std::make_shared<OrderListener>(2, call_order);
    editor.add_listener(o1);
    editor.add_listener(o2);

    AddTrack cmd2{main_seq_id, model::TrackKind::Audio, "A", std::nullopt};
    ASSERT_TRUE(test::is_ok(editor.execute(cmd2)));
    ASSERT_EQ(call_order.size(), 2U);
    EXPECT_EQ(call_order[0], 1);
    EXPECT_EQ(call_order[1], 2);

    // 6. Removed listener not called
    editor.remove_listener(o1.get());
    call_order.clear();
    AddTrack cmd3{main_seq_id, model::TrackKind::Video, "V", std::nullopt};
    ASSERT_TRUE(test::is_ok(editor.execute(cmd3)));
    ASSERT_EQ(call_order.size(), 1U);
    EXPECT_EQ(call_order[0], 2);

    // 7. Reentrant execution attempt gives InvalidArgument while outer commit succeeds
    auto reentrant = std::make_shared<ReentrantListener>(editor, main_seq_id);
    editor.add_listener(reentrant);

    AddTrack cmd4{main_seq_id, model::TrackKind::Video, "Outer", std::nullopt};
    auto outer_res = editor.execute(cmd4);
    ASSERT_TRUE(test::is_ok(outer_res));
    EXPECT_EQ(reentrant->reentrant_code, core::ErrorCode::InvalidArgument);
}

TEST(EditorTest, DeterminismAcrossIdenticallySeededEditors) {
    core::UuidGenerator gen1(12345ULL);
    core::UuidGenerator gen2(12345ULL);

    auto ed1_res = Editor::create(model::test::build_valid_project(gen1), gen1);
    auto ed2_res = Editor::create(model::test::build_valid_project(gen2), gen2);
    ASSERT_TRUE(test::is_ok(ed1_res));
    ASSERT_TRUE(test::is_ok(ed2_res));

    Editor& ed1 = *ed1_res.value();
    Editor& ed2 = *ed2_res.value();

    const auto main1 = ed1.snapshot()->main_sequence;
    const auto main2 = ed2.snapshot()->main_sequence;

    // 1. Add track
    AddTrack c1_1{main1, model::TrackKind::Video, "Det1", std::nullopt};
    AddTrack c1_2{main2, model::TrackKind::Video, "Det1", std::nullopt};
    auto r1_1 = ed1.execute(c1_1);
    auto r1_2 = ed2.execute(c1_2);
    ASSERT_TRUE(test::is_ok(r1_1));
    ASSERT_TRUE(test::is_ok(r1_2));
    EXPECT_EQ(*r1_1, *r1_2);

    // 2. Add marker
    AddMarker c2_1{main1, model::test::timeline_at_seconds(1), "M1",
                   core::Color{1.0f, 1.0f, 1.0f, 1.0f}};
    AddMarker c2_2{main2, model::test::timeline_at_seconds(1), "M1",
                   core::Color{1.0f, 1.0f, 1.0f, 1.0f}};
    auto r2_1 = ed1.execute(c2_1);
    auto r2_2 = ed2.execute(c2_2);
    ASSERT_TRUE(test::is_ok(r2_1));
    ASSERT_TRUE(test::is_ok(r2_2));
    EXPECT_EQ(*r2_1, *r2_2);

    // 3. Move track
    MoveTrack c3_1{main1, r1_1->created_tracks[0], 0U};
    MoveTrack c3_2{main2, r1_2->created_tracks[0], 0U};
    ASSERT_TRUE(test::is_ok(ed1.execute(c3_1)));
    ASSERT_TRUE(test::is_ok(ed2.execute(c3_2)));

    // 4. Set track properties
    SetTrackProperties c4_1{main1, r1_1->created_tracks[0], "RenamedDet", true, false};
    SetTrackProperties c4_2{main2, r1_2->created_tracks[0], "RenamedDet", true, false};
    ASSERT_TRUE(test::is_ok(ed1.execute(c4_1)));
    ASSERT_TRUE(test::is_ok(ed2.execute(c4_2)));

    // 5. Add media
    model::MediaAsset asset1;
    asset1.name = "det.png";
    asset1.path = "/det.png";
    asset1.kind = model::MediaKind::Image;
    model::MediaAsset asset2 = asset1;

    AddMedia c5_1{asset1};
    AddMedia c5_2{asset2};
    auto r5_1 = ed1.execute(c5_1);
    auto r5_2 = ed2.execute(c5_2);
    ASSERT_TRUE(test::is_ok(r5_1));
    ASSERT_TRUE(test::is_ok(r5_2));
    EXPECT_EQ(*r5_1, *r5_2);

    EXPECT_TRUE(model::identical(*ed1.snapshot(), *ed2.snapshot()));
}

TEST(EditorTest, ExecuteAfterCommitReturnsInvalidArgument) {
    core::UuidGenerator gen(920ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;

    auto tx_res = editor.begin_transaction("CommitTx");
    ASSERT_TRUE(test::is_ok(tx_res));
    auto& tx = *tx_res;

    AddTrack cmd1{main_seq_id, model::TrackKind::Video, "T1", std::nullopt};
    ASSERT_TRUE(test::is_ok(tx.execute(cmd1)));

    auto commit_res = tx.commit();
    ASSERT_TRUE(test::is_ok(commit_res));

    AddTrack cmd2{main_seq_id, model::TrackKind::Video, "T2", std::nullopt};
    auto exec_res = tx.execute(cmd2);
    ASSERT_FALSE(exec_res.has_value());
    EXPECT_EQ(exec_res.error().code(), core::ErrorCode::InvalidArgument);
    EXPECT_NE(exec_res.error().message().find("transaction is not open"), std::string_view::npos);
}

TEST(EditorTest,
     SecondCommitOnFinishedTransactionReturnsInvalidArgumentAndDoesNotCloseNewerTransaction) {
    core::UuidGenerator gen(921ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;

    // Begin tx1, commit tx1
    auto tx1_res = editor.begin_transaction("Tx1");
    ASSERT_TRUE(test::is_ok(tx1_res));
    auto& tx1 = *tx1_res;
    AddTrack cmd1{main_seq_id, model::TrackKind::Video, "T1", std::nullopt};
    ASSERT_TRUE(test::is_ok(tx1.execute(cmd1)));
    ASSERT_TRUE(test::is_ok(tx1.commit()));

    // Begin tx2
    auto tx2_res = editor.begin_transaction("Tx2");
    ASSERT_TRUE(test::is_ok(tx2_res));
    auto& tx2 = *tx2_res;

    // Call tx1.commit() again (error)
    auto second_commit_res = tx1.commit();
    ASSERT_FALSE(second_commit_res.has_value());
    EXPECT_EQ(second_commit_res.error().code(), core::ErrorCode::InvalidArgument);
    EXPECT_NE(second_commit_res.error().message().find("transaction is not open"),
              std::string_view::npos);

    // Then editor.begin_transaction(...) still fails with "transaction open"
    auto tx3_res = editor.begin_transaction("Tx3");
    ASSERT_FALSE(tx3_res.has_value());
    EXPECT_EQ(tx3_res.error().code(), core::ErrorCode::InvalidArgument);
    EXPECT_NE(tx3_res.error().message().find("transaction open"), std::string_view::npos);

    // And tx2 can still execute and commit successfully
    AddTrack cmd2{main_seq_id, model::TrackKind::Video, "T2", std::nullopt};
    ASSERT_TRUE(test::is_ok(tx2.execute(cmd2)));
    ASSERT_TRUE(test::is_ok(tx2.commit()));
}

TEST(EditorTest, ExecuteAfterRollbackReturnsInvalidArgument) {
    core::UuidGenerator gen(922ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;

    auto tx_res = editor.begin_transaction("RollbackTx");
    ASSERT_TRUE(test::is_ok(tx_res));
    auto& tx = *tx_res;

    tx.rollback();

    AddTrack cmd{main_seq_id, model::TrackKind::Video, "T", std::nullopt};
    auto exec_res = tx.execute(cmd);
    ASSERT_FALSE(exec_res.has_value());
    EXPECT_EQ(exec_res.error().code(), core::ErrorCode::InvalidArgument);
    EXPECT_NE(exec_res.error().message().find("transaction is not open"), std::string_view::npos);
}

TEST(EditorTest, BeginTransactionSucceedsAfterRejectedCommit) {
    core::UuidGenerator gen(923ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;
    const auto& track = editor.snapshot()->sequences.at(main_seq_id).tracks[0];
    const auto clip_id = track.clips[0].id;

    auto tx1_res = editor.begin_transaction("InvalidBatch");
    ASSERT_TRUE(test::is_ok(tx1_res));
    auto& tx1 = *tx1_res;

    InvalidClipDurationCommand bad_cmd{main_seq_id, track.id, clip_id};
    ASSERT_TRUE(test::is_ok(tx1.execute(bad_cmd)));

    auto commit_res = tx1.commit();
    ASSERT_FALSE(commit_res.has_value());
    EXPECT_EQ(commit_res.error().code(), core::ErrorCode::InvalidArgument);

    // After a rejected commit, a new begin_transaction succeeds
    auto tx2_res = editor.begin_transaction("NewTxAfterRejection");
    ASSERT_TRUE(test::is_ok(tx2_res));
    auto& tx2 = *tx2_res;

    AddTrack cmd{main_seq_id, model::TrackKind::Video, "ValidTrack", std::nullopt};
    ASSERT_TRUE(test::is_ok(tx2.execute(cmd)));
    ASSERT_TRUE(test::is_ok(tx2.commit()));
}

TEST(EditorTest, GeneratedIdsNeverCollideWithFixtureIds) {
    core::UuidGenerator gen(999ULL);
    auto ed_res = Editor::create(model::test::build_valid_project(gen), gen);
    ASSERT_TRUE(test::is_ok(ed_res));
    Editor& editor = *ed_res.value();

    const auto main_seq_id = editor.snapshot()->main_sequence;

    // Execute 40 AddTrack commands (alternating Video/Audio)
    for (int i = 0; i < 40; ++i) {
        const auto kind = (i % 2 == 0) ? model::TrackKind::Video : model::TrackKind::Audio;
        AddTrack cmd{main_seq_id, kind, "Track_" + std::to_string(i), std::nullopt};
        auto r = editor.execute(cmd);
        ASSERT_TRUE(test::is_ok(r));
    }

    // Execute 20 AddMarker commands (times i whole seconds, using model::test::timeline_at_seconds)
    for (int i = 0; i < 20; ++i) {
        AddMarker cmd{main_seq_id, model::test::timeline_at_seconds(i),
                      "Marker_" + std::to_string(i), core::Color{1.0f, 1.0f, 1.0f, 1.0f}};
        auto r = editor.execute(cmd);
        ASSERT_TRUE(test::is_ok(r));
    }

    // Assert that all track ids of the main sequence are pairwise distinct
    const auto& tracks = editor.snapshot()->sequences.at(main_seq_id).tracks;
    std::unordered_set<model::TrackId> track_ids;
    for (const auto& track : tracks) {
        EXPECT_TRUE(track_ids.insert(track.id).second);
    }
    EXPECT_EQ(track_ids.size(), tracks.size());

    // And model::validate(*editor.snapshot()) succeeds
    EXPECT_TRUE(test::is_ok(model::validate(*editor.snapshot())));
}

}  // namespace
}  // namespace nxtcut::commands
