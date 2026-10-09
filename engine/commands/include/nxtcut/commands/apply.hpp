#pragma once

#include <nxtcut/commands/change_set.hpp>
#include <nxtcut/core/result.hpp>
#include <nxtcut/model/project.hpp>

namespace nxtcut::commands {

/**
 * @brief Enforces canonical ordering across all tracks and sequences in the project.
 *
 * Places every track's clips in canonical order (ascending start ticks, ties broken by clip id)
 * and every sequence's markers in canonical order (ascending time ticks, ties broken by marker id)
 * using a stable sort.
 *
 * @param project The project document to normalize in-place.
 */
void normalize(model::Project& project);

/**
 * @brief Applies an ordered ChangeSet to a mutable Project.
 *
 * Precondition failure codes:
 * - Creating an entity that already exists: ErrorCode::AlreadyExists
 * - Modifying or removing an entity that is missing: ErrorCode::NotFound
 * - Lifecycle change with both or neither side set: ErrorCode::Internal
 * - Index out of bounds: ErrorCode::OutOfRange
 *
 * Canonical ordering is maintained across all clip and marker mutations.
 * Note: Non-atomic. The Editor applies to an isolated copy before committing state.
 *
 * @param project The project to mutate.
 * @param change_set The set of mutations to execute in order.
 * @return Success Status or ErrorCode on precondition violation.
 */
[[nodiscard]] core::Status apply(model::Project& project, const ChangeSet& change_set);

}  // namespace nxtcut::commands
