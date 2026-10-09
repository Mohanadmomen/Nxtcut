#include <nxtcut/commands/history.hpp>

#include <utility>

namespace nxtcut::commands {

History::History(std::size_t max_steps) : max_steps_(max_steps) {}

void History::push(ChangeSet change_set) {
    redo_stack_.clear();
    undo_stack_.push_back(std::move(change_set));
    if (undo_stack_.size() > max_steps_) {
        undo_stack_.pop_front();
    }
}

const ChangeSet* History::undo_top() const noexcept {
    if (undo_stack_.empty()) {
        return nullptr;
    }
    return &undo_stack_.back();
}

const ChangeSet* History::redo_top() const noexcept {
    if (redo_stack_.empty()) {
        return nullptr;
    }
    return &redo_stack_.back();
}

void History::commit_undo() {
    if (undo_stack_.empty()) {
        return;
    }
    redo_stack_.push_back(std::move(undo_stack_.back()));
    undo_stack_.pop_back();
}

void History::commit_redo() {
    if (redo_stack_.empty()) {
        return;
    }
    undo_stack_.push_back(std::move(redo_stack_.back()));
    redo_stack_.pop_back();
    if (undo_stack_.size() > max_steps_) {
        undo_stack_.pop_front();
    }
}

std::size_t History::undo_size() const noexcept {
    return undo_stack_.size();
}

std::size_t History::redo_size() const noexcept {
    return redo_stack_.size();
}

void History::clear() noexcept {
    undo_stack_.clear();
    redo_stack_.clear();
}

}  // namespace nxtcut::commands
