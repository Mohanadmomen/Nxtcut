#pragma once

#include <nxtcut/commands/apply.hpp>
#include <nxtcut/commands/change_set.hpp>
#include <nxtcut/commands/edit_command.hpp>
#include <nxtcut/commands/history.hpp>
#include <nxtcut/core/result.hpp>
#include <nxtcut/core/uuid.hpp>
#include <nxtcut/model/project.hpp>
#include <nxtcut/model/validation.hpp>

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nxtcut::commands {

/**
 * @brief Discriminator for edit lifecycle events.
 */
enum class EditEventKind {
    Commit,
    Undo,
    Redo,
};

/**
 * @brief Event payload delivered to EditListener observers.
 */
struct EditEvent {
    EditEventKind kind;
    const ChangeSet& changes;
    std::shared_ptr<const model::Project> project;
};

/**
 * @brief Interface for observing committed, undone, or redone document mutations.
 *
 * @note Thread safety: UI thread only. Listeners are invoked synchronously.
 */
class EditListener {
public:
    virtual ~EditListener() = default;
    EditListener() = default;
    EditListener(const EditListener&) = delete;
    EditListener& operator=(const EditListener&) = delete;
    EditListener(EditListener&&) = delete;
    EditListener& operator=(EditListener&&) = delete;

    virtual void on_edit(const EditEvent& event) = 0;
};

/**
 * @brief Configuration parameters for Editor initialization.
 */
struct EditorOptions {
    std::size_t max_history_steps = 200;
    bool validate_on_commit = true;
};

class Editor;

/**
 * @brief Move-only RAII transaction wrapping one or more command executions into one atomic
 * commit.
 *
 * Lifetime: The owning Editor must outlive the Transaction.
 * Destructor automatically rolls back if commit() was not called.
 *
 * @note Thread safety: UI thread only.
 */
class Transaction {
public:
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;

    Transaction(Transaction&& other) noexcept;
    Transaction& operator=(Transaction&& other) noexcept;

    ~Transaction();

    /**
     * @brief Executes a command against the transaction's working project.
     *
     * A failed build or apply leaves the transaction's working state unchanged.
     */
    template <EditCommand C>
    core::Result<EditReceipt> execute(const C& command);

    /**
     * @brief Commits the transaction, publishing state changes to the Editor.
     */
    [[nodiscard]] core::Result<EditReceipt> commit();

    /**
     * @brief Aborts the transaction and discards uncommitted changes.
     */
    void rollback() noexcept;

    /**
     * @brief Working project document reflecting currently accumulated edits.
     */
    [[nodiscard]] const model::Project& project() const noexcept { return working_project_; }

private:
    friend class Editor;
    explicit Transaction(Editor* editor, std::string label, model::Project initial_working_project);

    Editor* editor_{nullptr};
    std::string label_;
    model::Project working_project_;
    ChangeSet accumulated_changes_;
    bool committed_{false};
};

/**
 * @brief UI-thread document coordinator managing mutations, snapshots, history, and listeners.
 *
 * Snapshots returned by snapshot() are immutable and safe to read concurrently from any thread.
 *
 * @note Thread safety: UI thread only.
 */
class Editor {
public:
    struct ConstructToken {
    private:
        friend class Editor;
        explicit ConstructToken() = default;
    };

    explicit Editor(ConstructToken, model::Project initial, core::UuidGenerator& ids,
                    EditorOptions options);

    Editor(const Editor&) = delete;
    Editor& operator=(const Editor&) = delete;
    Editor(Editor&&) = delete;
    Editor& operator=(Editor&&) = delete;

    ~Editor() = default;

    /**
     * @brief Factory creating an Editor instance.
     *
     * Precondition: ids generator must outlive the editor.
     *
     * @param initial Initial project document (validated and normalized).
     * @param ids ID generator reference.
     * @param options History and validation configuration.
     * @return Unique pointer to Editor or ErrorCode::InvalidArgument on validation failure.
     */
    [[nodiscard]] static core::Result<std::unique_ptr<Editor>> create(model::Project initial,
                                                                      core::UuidGenerator& ids,
                                                                      EditorOptions options = {});

    /**
     * @brief Returns the current immutable project snapshot. Never null.
     */
    [[nodiscard]] std::shared_ptr<const model::Project> snapshot() const noexcept {
        return current_snapshot_;
    }

    /**
     * @brief Non-owning reference to the current project snapshot.
     */
    [[nodiscard]] const model::Project& project() const noexcept { return *current_snapshot_; }

    /**
     * @brief Atomically executes a single EditCommand.
     */
    template <EditCommand C>
    core::Result<EditReceipt> execute(const C& command) {
        auto tx_res = begin_transaction(std::string(command.label()));
        if (!tx_res.has_value()) {
            return tl::unexpected(tx_res.error());
        }
        auto exec_res = tx_res->execute(command);
        if (!exec_res.has_value()) {
            return tl::unexpected(exec_res.error());
        }
        return tx_res->commit();
    }

    /**
     * @brief Begins a multi-command editing transaction.
     */
    [[nodiscard]] core::Result<Transaction> begin_transaction(std::string label);

    /**
     * @brief Undoes the topmost history operation.
     */
    core::Status undo();

    /**
     * @brief Redoes the topmost undone operation.
     */
    core::Status redo();

    /**
     * @brief Checks if an undo operation is available.
     */
    [[nodiscard]] bool can_undo() const noexcept { return history_.undo_size() > 0; }

    /**
     * @brief Checks if a redo operation is available.
     */
    [[nodiscard]] bool can_redo() const noexcept { return history_.redo_size() > 0; }

    /**
     * @brief Human-readable label for the next undo action, or nullopt if none.
     */
    [[nodiscard]] std::optional<std::string_view> undo_label() const noexcept;

    /**
     * @brief Human-readable label for the next redo action, or nullopt if none.
     */
    [[nodiscard]] std::optional<std::string_view> redo_label() const noexcept;

    /**
     * @brief Registers an edit event listener.
     */
    void add_listener(std::shared_ptr<EditListener> listener);

    /**
     * @brief Unregisters an edit event listener.
     */
    void remove_listener(const EditListener* listener);

    /**
     * @brief Reference to the injected UUID generator.
     */
    [[nodiscard]] core::UuidGenerator& uuid_generator() const noexcept { return ids_; }

private:
    friend class Transaction;

    [[nodiscard]] core::Result<EditReceipt> commit_transaction(Transaction& tx);
    void close_transaction() noexcept;
    void notify_listeners(const EditEvent& event);

    core::UuidGenerator& ids_;
    EditorOptions options_;
    std::shared_ptr<const model::Project> current_snapshot_;
    History history_;
    std::vector<std::shared_ptr<EditListener>> listeners_;
    bool transaction_open_{false};
    bool is_notifying_{false};
};

template <EditCommand C>
core::Result<EditReceipt> Transaction::execute(const C& command) {
    if (editor_ == nullptr || committed_) {
        return core::make_error(core::ErrorCode::InvalidArgument, "transaction is not open");
    }
    auto build_res = command.build(working_project_, editor_->uuid_generator());
    if (!build_res.has_value()) {
        return tl::unexpected(build_res.error());
    }

    auto& cs = *build_res;
    if (cs.empty()) {
        return receipt_of(cs);
    }

    // Apply to copy first so a failure leaves working_project_ unmodified
    model::Project copy = working_project_;
    auto apply_res = apply(copy, cs);
    if (!apply_res.has_value()) {
        return tl::unexpected(apply_res.error());
    }

    working_project_ = std::move(copy);
    for (auto& ch : cs.changes) {
        accumulated_changes_.changes.push_back(std::move(ch));
    }
    return receipt_of(cs);
}

}  // namespace nxtcut::commands
