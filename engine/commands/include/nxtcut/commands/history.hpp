#pragma once

#include <nxtcut/commands/change_set.hpp>

#include <cstddef>
#include <deque>

namespace nxtcut::commands {

/**
 * @brief Manages bounded undo and redo stacks of ChangeSet entries.
 *
 * Precondition: max_steps >= 1.
 *
 * @note Thread safety: Main-thread / UI-thread only (not thread-safe).
 */
class History {
public:
    /**
     * @brief Constructs a History manager with a fixed maximum history capacity.
     *
     * @param max_steps Maximum number of undo states retained (must be >= 1).
     */
    explicit History(std::size_t max_steps);

    /**
     * @brief Pushes a new ChangeSet onto the undo stack.
     *
     * Clears the redo stack. If the undo stack size exceeds max_steps, the oldest
     * undo entry is discarded.
     */
    void push(ChangeSet change_set);

    /**
     * @brief Returns a non-owning pointer to the top undo entry, or nullptr if empty.
     */
    [[nodiscard]] const ChangeSet* undo_top() const noexcept;

    /**
     * @brief Returns a non-owning pointer to the top redo entry, or nullptr if empty.
     */
    [[nodiscard]] const ChangeSet* redo_top() const noexcept;

    /**
     * @brief Moves the topmost undo entry onto the redo stack.
     *
     * Precondition: undo_top() != nullptr.
     */
    void commit_undo();

    /**
     * @brief Moves the topmost redo entry onto the undo stack.
     *
     * Precondition: redo_top() != nullptr.
     */
    void commit_redo();

    /**
     * @brief Number of available undo operations.
     */
    [[nodiscard]] std::size_t undo_size() const noexcept;

    /**
     * @brief Number of available redo operations.
     */
    [[nodiscard]] std::size_t redo_size() const noexcept;

    /**
     * @brief Clears both undo and redo stacks.
     */
    void clear() noexcept;

private:
    std::size_t max_steps_{200};
    std::deque<ChangeSet> undo_stack_;
    std::deque<ChangeSet> redo_stack_;
};

}  // namespace nxtcut::commands
