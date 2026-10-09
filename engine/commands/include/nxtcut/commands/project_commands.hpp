#pragma once

#include <nxtcut/commands/change_set.hpp>
#include <nxtcut/core/result.hpp>
#include <nxtcut/core/uuid.hpp>
#include <nxtcut/model/ids.hpp>
#include <nxtcut/model/project.hpp>

#include <optional>
#include <string>

namespace nxtcut::commands {

/**
 * @brief Command to update top-level project document properties.
 */
struct SetProjectProperties {
    std::optional<std::string> name;
    std::optional<model::SequenceId> main_sequence;

    [[nodiscard]] std::string label() const { return "Set Project Properties"; }
    [[nodiscard]] core::Result<ChangeSet> build(const model::Project& project,
                                                core::UuidGenerator& ids) const;
};

}  // namespace nxtcut::commands
