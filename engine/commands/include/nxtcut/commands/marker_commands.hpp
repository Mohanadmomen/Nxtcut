#pragma once

#include <nxtcut/commands/change_set.hpp>
#include <nxtcut/core/color.hpp>
#include <nxtcut/core/result.hpp>
#include <nxtcut/core/uuid.hpp>
#include <nxtcut/model/ids.hpp>
#include <nxtcut/model/project.hpp>
#include <nxtcut/model/sequence.hpp>
#include <nxtcut/model/time_coords.hpp>

#include <string>

namespace nxtcut::commands {

/**
 * @brief Command to add a timeline marker to a sequence.
 */
struct AddMarker {
    model::SequenceId sequence;
    model::TimelineTime time;
    std::string text;
    core::Color color{core::Color{1.0f, 1.0f, 1.0f, 1.0f}};

    [[nodiscard]] std::string label() const { return "Add Marker"; }
    [[nodiscard]] core::Result<ChangeSet> build(const model::Project& project,
                                                core::UuidGenerator& ids) const;
};

/**
 * @brief Command to delete an existing marker from a sequence.
 */
struct RemoveMarker {
    model::SequenceId sequence;
    model::MarkerId marker;

    [[nodiscard]] std::string label() const { return "Remove Marker"; }
    [[nodiscard]] core::Result<ChangeSet> build(const model::Project& project,
                                                core::UuidGenerator& ids) const;
};

}  // namespace nxtcut::commands
