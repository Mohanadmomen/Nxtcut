#include <nxtcut/commands/change_set.hpp>

#include <type_traits>
#include <utility>
#include <variant>

namespace nxtcut::commands {

ChangeSet inverse(const ChangeSet& change_set) {
    ChangeSet result;
    result.label = change_set.label;
    result.changes.reserve(change_set.changes.size());

    for (auto it = change_set.changes.rbegin(); it != change_set.changes.rend(); ++it) {
        std::visit(
            [&result](const auto& change) {
                using T = std::decay_t<decltype(change)>;
                if constexpr (std::is_same_v<T, ProjectPropertiesChange>) {
                    result.changes.push_back(ProjectPropertiesChange{change.after, change.before});
                } else if constexpr (std::is_same_v<T, MediaChange>) {
                    result.changes.push_back(MediaChange{change.id, change.after, change.before});
                } else if constexpr (std::is_same_v<T, SequenceChange>) {
                    result.changes.push_back(
                        SequenceChange{change.id, change.after, change.before});
                } else if constexpr (std::is_same_v<T, SequenceSettingsChange>) {
                    result.changes.push_back(
                        SequenceSettingsChange{change.id, change.after, change.before});
                } else if constexpr (std::is_same_v<T, TrackChange>) {
                    result.changes.push_back(
                        TrackChange{change.sequence, change.id, change.after, change.before});
                } else if constexpr (std::is_same_v<T, TrackMove>) {
                    result.changes.push_back(
                        TrackMove{change.sequence, change.id, change.to_index, change.from_index});
                } else if constexpr (std::is_same_v<T, TrackPropertiesChange>) {
                    result.changes.push_back(TrackPropertiesChange{change.sequence, change.id,
                                                                   change.after, change.before});
                } else if constexpr (std::is_same_v<T, ClipChange>) {
                    result.changes.push_back(ClipChange{change.sequence, change.track, change.id,
                                                        change.after, change.before});
                } else if constexpr (std::is_same_v<T, MarkerChange>) {
                    result.changes.push_back(
                        MarkerChange{change.sequence, change.id, change.after, change.before});
                }
            },
            *it);
    }

    return result;
}

EditReceipt receipt_of(const ChangeSet& change_set) {
    EditReceipt receipt;
    for (const auto& change_var : change_set.changes) {
        std::visit(
            [&receipt](const auto& change) {
                using T = std::decay_t<decltype(change)>;
                if constexpr (std::is_same_v<T, SequenceChange>) {
                    if (!change.before.has_value() && change.after.has_value()) {
                        receipt.created_sequences.push_back(change.id);
                    }
                } else if constexpr (std::is_same_v<T, TrackChange>) {
                    if (!change.before.has_value() && change.after.has_value()) {
                        receipt.created_tracks.push_back(change.id);
                    }
                } else if constexpr (std::is_same_v<T, ClipChange>) {
                    if (!change.before.has_value() && change.after.has_value()) {
                        receipt.created_clips.push_back(change.id);
                    }
                } else if constexpr (std::is_same_v<T, MarkerChange>) {
                    if (!change.before.has_value() && change.after.has_value()) {
                        receipt.created_markers.push_back(change.id);
                    }
                } else if constexpr (std::is_same_v<T, MediaChange>) {
                    if (!change.before.has_value() && change.after.has_value()) {
                        receipt.created_media.push_back(change.id);
                    }
                }
            },
            change_var);
    }
    return receipt;
}

}  // namespace nxtcut::commands
