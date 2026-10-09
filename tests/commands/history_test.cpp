#include <nxtcut/commands/history.hpp>

#include <gtest/gtest.h>

#include <string>

namespace nxtcut::commands {
namespace {

TEST(HistoryTest, EmptyStackAccessorsReturnNull) {
    History h(5);
    EXPECT_EQ(h.undo_size(), 0U);
    EXPECT_EQ(h.redo_size(), 0U);
    EXPECT_EQ(h.undo_top(), nullptr);
    EXPECT_EQ(h.redo_top(), nullptr);
}

TEST(HistoryTest, CapacityLimitRetainsNewestEntries) {
    History h(3);

    for (int i = 1; i <= 5; ++i) {
        ChangeSet cs;
        cs.label = "Step" + std::to_string(i);
        h.push(cs);
    }

    // Size must be capped at 3
    EXPECT_EQ(h.undo_size(), 3U);
    EXPECT_EQ(h.redo_size(), 0U);

    // Top must be Step 5
    ASSERT_NE(h.undo_top(), nullptr);
    EXPECT_EQ(h.undo_top()->label, "Step5");

    // After commit_undo, top is Step 4
    h.commit_undo();
    EXPECT_EQ(h.undo_size(), 2U);
    EXPECT_EQ(h.redo_size(), 1U);
    ASSERT_NE(h.undo_top(), nullptr);
    EXPECT_EQ(h.undo_top()->label, "Step4");

    // After another commit_undo, top is Step 3 (oldest retained)
    h.commit_undo();
    EXPECT_EQ(h.undo_size(), 1U);
    EXPECT_EQ(h.redo_size(), 2U);
    ASSERT_NE(h.undo_top(), nullptr);
    EXPECT_EQ(h.undo_top()->label, "Step3");

    // Third commit_undo empties undo stack
    h.commit_undo();
    EXPECT_EQ(h.undo_size(), 0U);
    EXPECT_EQ(h.undo_top(), nullptr);
}

TEST(HistoryTest, PushAfterUndoClearsRedo) {
    History h(5);

    ChangeSet cs1;
    cs1.label = "Action1";
    h.push(cs1);

    ChangeSet cs2;
    cs2.label = "Action2";
    h.push(cs2);

    h.commit_undo();
    EXPECT_EQ(h.undo_size(), 1U);
    EXPECT_EQ(h.redo_size(), 1U);
    ASSERT_NE(h.redo_top(), nullptr);
    EXPECT_EQ(h.redo_top()->label, "Action2");

    // Pushing new action must discard redo stack
    ChangeSet cs3;
    cs3.label = "Action3";
    h.push(cs3);

    EXPECT_EQ(h.undo_size(), 2U);
    EXPECT_EQ(h.redo_size(), 0U);
    EXPECT_EQ(h.redo_top(), nullptr);
    ASSERT_NE(h.undo_top(), nullptr);
    EXPECT_EQ(h.undo_top()->label, "Action3");
}

TEST(HistoryTest, CommitUndoAndCommitRedoMoveEntriesCorrectly) {
    History h(5);

    ChangeSet cs1;
    cs1.label = "One";
    h.push(cs1);

    ChangeSet cs2;
    cs2.label = "Two";
    h.push(cs2);

    EXPECT_EQ(h.undo_size(), 2U);
    EXPECT_EQ(h.redo_size(), 0U);

    h.commit_undo();
    EXPECT_EQ(h.undo_size(), 1U);
    EXPECT_EQ(h.redo_size(), 1U);
    ASSERT_NE(h.undo_top(), nullptr);
    EXPECT_EQ(h.undo_top()->label, "One");
    ASSERT_NE(h.redo_top(), nullptr);
    EXPECT_EQ(h.redo_top()->label, "Two");

    h.commit_redo();
    EXPECT_EQ(h.undo_size(), 2U);
    EXPECT_EQ(h.redo_size(), 0U);
    ASSERT_NE(h.undo_top(), nullptr);
    EXPECT_EQ(h.undo_top()->label, "Two");
    EXPECT_EQ(h.redo_top(), nullptr);
}

TEST(HistoryTest, ClearEmptiesBothStacks) {
    History h(5);
    ChangeSet cs;
    cs.label = "A";
    h.push(cs);
    h.commit_undo();

    EXPECT_EQ(h.redo_size(), 1U);
    h.clear();
    EXPECT_EQ(h.undo_size(), 0U);
    EXPECT_EQ(h.redo_size(), 0U);
    EXPECT_EQ(h.undo_top(), nullptr);
    EXPECT_EQ(h.redo_top(), nullptr);
}

}  // namespace
}  // namespace nxtcut::commands
