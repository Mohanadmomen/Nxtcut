#pragma once

#include <nxtcut/commands/change_set.hpp>
#include <nxtcut/core/result.hpp>
#include <nxtcut/core/uuid.hpp>
#include <nxtcut/model/project.hpp>

#include <concepts>
#include <string>

namespace nxtcut::commands {

/**
 * @brief Concept specifying the contract for a transactional editing command.
 *
 * An EditCommand provides a pure build() function that examines the current project
 * and outputs a ChangeSet describing the forward mutations, without mutating the
 * project directly. Undo behavior is derived generically via inverse().
 */
template <class C>
concept EditCommand = requires(const C& c, const model::Project& p, core::UuidGenerator& g) {
    { c.build(p, g) } -> std::same_as<core::Result<ChangeSet>>;
    { c.label() } -> std::convertible_to<std::string>;
};

}  // namespace nxtcut::commands
