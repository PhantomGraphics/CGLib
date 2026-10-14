#include <gtest/gtest.h>
#include "../History/UndoStack.h"
#include <string>
#include <vector>

using Phantom::Editor::UndoStack;

TEST(UndoStackTest, EmptyHistoryLeavesCurrentStateUnchanged) {
    UndoStack<std::string> history;
    std::string current = "present";
    EXPECT_FALSE(history.undo(current));
    EXPECT_FALSE(history.redo(current));
    EXPECT_EQ(current, "present");
    EXPECT_EQ(history.undoCount(), 0u);
    EXPECT_EQ(history.redoCount(), 0u);
}

TEST(UndoStackTest, UndoRedoRoundTripPreservesFullSnapshot) {
    struct Snapshot {
        std::string name;
        std::vector<int> selection;
    };
    UndoStack<Snapshot> history;
    history.push({"before", {2, 1}});
    Snapshot current = {"after", {3, 4}};
    ASSERT_TRUE(history.undo(current));
    EXPECT_EQ(current.name, "before");
    EXPECT_EQ(current.selection, (std::vector<int>{2, 1}));
    ASSERT_TRUE(history.redo(current));
    EXPECT_EQ(current.name, "after");
    EXPECT_EQ(current.selection, (std::vector<int>{3, 4}));
    EXPECT_EQ(history.undoCount(), 1u);
    EXPECT_EQ(history.redoCount(), 0u);
}

TEST(UndoStackTest, LimitDropsOldestAndRedoKeepsOrder) {
    UndoStack<int> history(2);
    for (int before : {0, 1, 2}) history.push(before);
    EXPECT_EQ(history.undoCount(), 2u);
    int current = 3;
    ASSERT_TRUE(history.undo(current));
    EXPECT_EQ(current, 2);
    ASSERT_TRUE(history.undo(current));
    EXPECT_EQ(current, 1);
    EXPECT_FALSE(history.undo(current));
    EXPECT_EQ(current, 1);
    ASSERT_TRUE(history.redo(current));
    EXPECT_EQ(current, 2);
    ASSERT_TRUE(history.redo(current));
    EXPECT_EQ(current, 3);
    EXPECT_FALSE(history.redo(current));
    EXPECT_EQ(history.undoCount(), 2u);
}

TEST(UndoStackTest, NewEditDropsRedoBranchAndClearDropsBothHistories) {
    UndoStack<int> history;
    history.push(0);
    history.push(1);
    int current = 2;
    ASSERT_TRUE(history.undo(current));
    ASSERT_EQ(history.redoCount(), 1u);
    history.push(current);
    current = 9;
    EXPECT_EQ(history.redoCount(), 0u);
    EXPECT_FALSE(history.redo(current));
    ASSERT_TRUE(history.undo(current));
    EXPECT_EQ(current, 1);
    EXPECT_EQ(history.undoCount(), 1u);
    EXPECT_EQ(history.redoCount(), 1u);
    history.clear();
    EXPECT_EQ(history.undoCount(), 0u);
    EXPECT_EQ(history.redoCount(), 0u);
    EXPECT_FALSE(history.undo(current));
    EXPECT_FALSE(history.redo(current));
    EXPECT_EQ(current, 1);
}

TEST(UndoStackTest, ZeroLimitDisablesHistoryAndDefaultLimitIsOneHundred) {
    UndoStack<int> disabled(0);
    disabled.push(0);
    int current = 1;
    EXPECT_EQ(disabled.undoCount(), 0u);
    EXPECT_FALSE(disabled.undo(current));
    EXPECT_FALSE(disabled.redo(current));
    EXPECT_EQ(current, 1);
    UndoStack<int> defaults;
    for (int i = 0; i < 101; ++i) defaults.push(i);
    EXPECT_EQ(defaults.undoCount(), 100u);
}
