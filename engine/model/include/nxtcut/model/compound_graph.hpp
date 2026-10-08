#pragma once

#include <nxtcut/model/ids.hpp>
#include <nxtcut/model/project.hpp>

#include <optional>
#include <vector>

namespace nxtcut::model {

/**
 * @brief Detects directed cycles formed by nested compound clips within a project.
 *
 * Traverses the directed graph where an edge sequence A -> sequence B indicates that
 * sequence A contains a compound clip referencing sequence B. Non-existent sequences
 * are ignored during graph traversal. Parallel edges between sequences are deduplicated
 * preserving first-seen order.
 *
 * find_compound_cycles reports ONE cycle per back edge of the depth-first search
 * (parallel edges count once); it does not enumerate every possible simple cycle;
 * after one cycle is fixed, validating again may reveal another.
 *
 * Implemented using an iterative depth-first search with explicit color states and
 * an explicit stack to prevent stack overflow on deeply nested hierarchies.
 *
 * @param project The project document to analyze.
 * @return A vector of detected cycles, each represented as a vector of sequence IDs
 *         (a self-loop returns a path of length 1). Returns an empty vector if acyclic.
 */
[[nodiscard]] std::vector<std::vector<SequenceId>> find_compound_cycles(const Project& project);

/**
 * @brief Detects directed cycles formed by nested compound clips within a project.
 *
 * Returns the first cycle detected by find_compound_cycles(project), or std::nullopt
 * if the compound hierarchy is acyclic.
 *
 * @param project The project document to analyze.
 * @return If a cycle is detected, returns the cycle sequence IDs path without repeating
 *         the start node at the end (a self-loop returns a path of length 1). Returns
 *         std::nullopt if the compound hierarchy is an acyclic DAG.
 */
[[nodiscard]] std::optional<std::vector<SequenceId>> find_compound_cycle(const Project& project);

/**
 * @brief Checks whether adding a compound clip referencing child into parent would form a cycle.
 *
 * Returns true if child == parent (self-reference), or if parent is already reachable from
 * child through existing compound clip relationships.
 *
 * @param project The current project document.
 * @param parent Sequence into which the compound clip would be placed.
 * @param child Sequence referenced by the candidate compound clip.
 * @return True if insertion would introduce a cycle, false if safe.
 */
[[nodiscard]] bool would_create_cycle(const Project& project, SequenceId parent, SequenceId child);

}  // namespace nxtcut::model
