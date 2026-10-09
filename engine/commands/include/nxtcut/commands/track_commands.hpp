#pragma once

#include <nxtcut/commands/change_set.hpp>
#include <nxtcut/core/result.hpp>
#include <nxtcut/core/uuid.hpp>
#include <nxtcut/model/ids.hpp>
#include <nxtcut/model/project.hpp>
#include <nxtcut/model/track.hpp>

#include <cstddef>
#include <optional>
#include <string>

namespace nxtcut::commands {

/**
 * @brief Command to add a new track to a sequence.
 */
struct AddTrack {
    model::SequenceId sequence;
    model::TrackKind kind{model::TrackKind::Video};
    std::string name;
    std::optional<std::size_t> index;

    [[nodiscard]] std::string label() const { return "Add Track"; }
    [[nodiscard]] core::Result<ChangeSet> build(const model::Project& project,
                                                core::UuidGenerator& ids) const;
};

/**
 * @brief Command to remove an existing track from a sequence.
 */
struct RemoveTrack {
    model::SequenceId sequence;
    model::TrackId track;

    [[nodiscard]] std::string label() const { return "Remove Track"; }
    [[nodiscard]] core::Result<ChangeSet> build(const model::Project& project,
                                                core::UuidGenerator& ids) const;
};

/**
 * @brief Command to reorder a track within a sequence.
 */
struct MoveTrack {
    model::SequenceId sequence;
    model::TrackId track;
    std::size_t new_index{0};

    [[nodiscard]] std::string label() const { return "Move Track"; }
    [[nodiscard]] core::Result<ChangeSet> build(const model::Project& project,
                                                core::UuidGenerator& ids) const;
};

/**
 * @brief Command to modify track properties (name, enabled, locked).
 */
struct SetTrackProperties {
    model::SequenceId sequence;
    model::TrackId track;
    std::optional<std::string> name;
    std::optional<bool> enabled;
    std::optional<bool> locked;

    [[nodiscard]] std::string label() const { return "Set Track Properties"; }
    [[nodiscard]] core::Result<ChangeSet> build(const model::Project& project,
                                                core::UuidGenerator& ids) const;
};

}  // namespace nxtcut::commands
