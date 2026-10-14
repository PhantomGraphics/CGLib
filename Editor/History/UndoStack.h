#pragma once

#include <cstddef>
#include <utility>
#include <vector>

namespace Phantom::Editor {

// Snapshot-based Undo/Redo. The host captures the
// state *before* a mutating command and push()es it; undo()/redo() swap the given current state with the
// recorded one. T must be copyable. Nothing here knows what T contains.
template <class T>
class UndoStack {
public:
    explicit UndoStack(size_t limit = 100) : limit_(limit) {}

    // Records `before` as the state to return to. Clears the redo history.
    void push(T before) {
        redo_.clear();
        undo_.push_back(std::move(before));
        if (undo_.size() > limit_) undo_.erase(undo_.begin());
    }

    // Replaces `current` with the previous state (and remembers `current` for redo). False when empty.
    bool undo(T& current) {
        if (undo_.empty()) return false;
        redo_.push_back(std::move(current));
        current = std::move(undo_.back());
        undo_.pop_back();
        return true;
    }

    bool redo(T& current) {
        if (redo_.empty()) return false;
        undo_.push_back(std::move(current));
        current = std::move(redo_.back());
        redo_.pop_back();
        return true;
    }

    // Empties both stacks (open / new / import are Undo boundaries).
    void clear() { undo_.clear(); redo_.clear(); }

    size_t undoCount() const { return undo_.size(); }
    size_t redoCount() const { return redo_.size(); }

private:
    size_t         limit_;
    std::vector<T> undo_;
    std::vector<T> redo_;
};

} // namespace Phantom::Editor
