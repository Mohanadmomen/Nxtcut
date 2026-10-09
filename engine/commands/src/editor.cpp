#include <nxtcut/commands/editor.hpp>

#include <algorithm>
#include <utility>

namespace nxtcut::commands {

Transaction::Transaction(Editor* editor, std::string label, model::Project initial_working_project)
    : editor_(editor), label_(label), working_project_(std::move(initial_working_project)) {
    accumulated_changes_.label = std::move(label);
}

Transaction::Transaction(Transaction&& other) noexcept
    : editor_(other.editor_),
      label_(std::move(other.label_)),
      working_project_(std::move(other.working_project_)),
      accumulated_changes_(std::move(other.accumulated_changes_)),
      committed_(other.committed_) {
    other.editor_ = nullptr;
    other.committed_ = true;
}

Transaction& Transaction::operator=(Transaction&& other) noexcept {
    if (this != &other) {
        if (!committed_ && editor_ != nullptr) {
            rollback();
        }
        editor_ = other.editor_;
        label_ = std::move(other.label_);
        working_project_ = std::move(other.working_project_);
        accumulated_changes_ = std::move(other.accumulated_changes_);
        committed_ = other.committed_;

        other.editor_ = nullptr;
        other.committed_ = true;
    }
    return *this;
}

Transaction::~Transaction() {
    if (!committed_ && editor_ != nullptr) {
        rollback();
    }
}

core::Result<EditReceipt> Transaction::commit() {
    if (editor_ == nullptr || committed_) {
        return core::make_error(core::ErrorCode::InvalidArgument, "transaction is not open");
    }
    committed_ = true;
    return editor_->commit_transaction(*this);
}

void Transaction::rollback() noexcept {
    if (!committed_ && editor_ != nullptr) {
        editor_->close_transaction();
        committed_ = true;
    }
}

Editor::Editor(ConstructToken, model::Project initial, core::UuidGenerator& ids,
               EditorOptions options)
    : ids_(ids),
      options_(options),
      current_snapshot_(std::make_shared<const model::Project>(std::move(initial))),
      history_(options.max_history_steps) {}

core::Result<std::unique_ptr<Editor>> Editor::create(model::Project initial,
                                                     core::UuidGenerator& ids,
                                                     EditorOptions options) {
    if (options.max_history_steps == 0) {
        return core::make_error(core::ErrorCode::InvalidArgument,
                                "max_history_steps must be greater than zero");
    }

    auto valid_res = model::validate(initial);
    if (!valid_res.has_value()) {
        return core::make_error(core::ErrorCode::InvalidArgument, valid_res.error().message());
    }

    commands::normalize(initial);

    return std::make_unique<Editor>(ConstructToken{}, std::move(initial), ids, options);
}

core::Result<Transaction> Editor::begin_transaction(std::string label) {
    if (is_notifying_) {
        return core::make_error(core::ErrorCode::InvalidArgument, "re-entrant");
    }
    if (transaction_open_) {
        return core::make_error(core::ErrorCode::InvalidArgument, "transaction open");
    }
    transaction_open_ = true;
    return Transaction(this, std::move(label), *current_snapshot_);
}

core::Result<EditReceipt> Editor::commit_transaction(Transaction& tx) {
    transaction_open_ = false;

    if (tx.accumulated_changes_.empty()) {
        return EditReceipt{};
    }

    if (options_.validate_on_commit) {
        auto valid_res = model::validate(tx.working_project_);
        if (!valid_res.has_value()) {
            return tl::unexpected(valid_res.error().with_context("edit rejected"));
        }
    }

    current_snapshot_ = std::make_shared<const model::Project>(std::move(tx.working_project_));
    EditReceipt receipt = receipt_of(tx.accumulated_changes_);
    history_.push(std::move(tx.accumulated_changes_));

    notify_listeners(EditEvent{EditEventKind::Commit, *history_.undo_top(), current_snapshot_});
    return receipt;
}

void Editor::close_transaction() noexcept {
    transaction_open_ = false;
}

core::Status Editor::undo() {
    if (is_notifying_) {
        return core::make_error(core::ErrorCode::InvalidArgument, "re-entrant");
    }
    if (transaction_open_) {
        return core::make_error(core::ErrorCode::InvalidArgument, "transaction open");
    }
    if (!can_undo()) {
        return core::make_error(core::ErrorCode::NotFound, "nothing to undo");
    }

    const ChangeSet* top = history_.undo_top();
    ChangeSet inv = inverse(*top);

    model::Project copy = *current_snapshot_;
    auto status = apply(copy, inv);
    if (!status.has_value()) {
        return status;
    }

    current_snapshot_ = std::make_shared<const model::Project>(std::move(copy));
    history_.commit_undo();

    notify_listeners(EditEvent{EditEventKind::Undo, inv, current_snapshot_});
    return core::Status{};
}

core::Status Editor::redo() {
    if (is_notifying_) {
        return core::make_error(core::ErrorCode::InvalidArgument, "re-entrant");
    }
    if (transaction_open_) {
        return core::make_error(core::ErrorCode::InvalidArgument, "transaction open");
    }
    if (!can_redo()) {
        return core::make_error(core::ErrorCode::NotFound, "nothing to redo");
    }

    const ChangeSet* top = history_.redo_top();
    ChangeSet redo_cs = *top;

    model::Project copy = *current_snapshot_;
    auto status = apply(copy, redo_cs);
    if (!status.has_value()) {
        return status;
    }

    current_snapshot_ = std::make_shared<const model::Project>(std::move(copy));
    history_.commit_redo();

    notify_listeners(EditEvent{EditEventKind::Redo, redo_cs, current_snapshot_});
    return core::Status{};
}

std::optional<std::string_view> Editor::undo_label() const noexcept {
    const auto* top = history_.undo_top();
    if (top == nullptr) {
        return std::nullopt;
    }
    return top->label;
}

std::optional<std::string_view> Editor::redo_label() const noexcept {
    const auto* top = history_.redo_top();
    if (top == nullptr) {
        return std::nullopt;
    }
    return top->label;
}

void Editor::add_listener(std::shared_ptr<EditListener> listener) {
    listeners_.push_back(std::move(listener));
}

void Editor::remove_listener(const EditListener* listener) {
    if (listener == nullptr) {
        return;
    }
    listeners_.erase(std::remove_if(listeners_.begin(), listeners_.end(),
                                    [listener](const std::shared_ptr<EditListener>& l) {
                                        return l.get() == listener;
                                    }),
                     listeners_.end());
}

void Editor::notify_listeners(const EditEvent& event) {
    struct NotificationGuard {
        bool& flag;
        explicit NotificationGuard(bool& f) : flag(f) { flag = true; }
        ~NotificationGuard() { flag = false; }
    } guard(is_notifying_);

    auto listeners_copy = listeners_;
    for (const auto& l : listeners_copy) {
        if (l != nullptr) {
            auto it = std::find(listeners_.begin(), listeners_.end(), l);
            if (it != listeners_.end()) {
                l->on_edit(event);
            }
        }
    }
}

}  // namespace nxtcut::commands
