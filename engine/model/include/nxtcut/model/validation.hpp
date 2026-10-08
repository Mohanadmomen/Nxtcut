#pragma once

#include <nxtcut/core/result.hpp>
#include <nxtcut/model/project.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace nxtcut::model {

/**
 * @brief Categorized validation codes indicating structural or semantic document inconsistencies.
 */
enum class ValidationCode : std::uint8_t {
    IdMismatch,
    DuplicateId,
    MissingMainSequence,
    InvalidCanvas,
    InvalidDuration,
    NegativeStart,
    ClipRangeOverflow,
    ClipOverlap,
    ClipKindMismatch,
    MissingMedia,
    MediaKindMismatch,
    SourceRangeOutOfBounds,
    LinkIncomplete,
    LinkAcrossSequences,
    MissingSequence,
    /**
     * @note find_compound_cycles reports ONE cycle per back edge of the depth-first search
     * (parallel edges count once); it does not enumerate every possible simple cycle;
     * after one cycle is fixed, validating again may reveal another.
     */
    CompoundCycle,
};

/**
 * @brief Converts a ValidationCode enumerator to its string name.
 */
[[nodiscard]] std::string_view to_string(ValidationCode code) noexcept;

/**
 * @brief Represents a single validation problem detected within a project document.
 */
struct ValidationIssue {
    ValidationCode code;
    std::string path;
    std::string message;
};

/**
 * @brief Validates a project document, collecting all issues without stopping at the first failure.
 *
 * Traversal and reporting are strictly deterministic:
 * Project-level issues appear first (missing main sequence, then media map IdMismatch in map
 * order), followed by sequences in map key order (sequence IdMismatch, InvalidCanvas, tracks in
 * vector order: duplicate track ID, clips in vector order: duplicate clip ID, InvalidDuration,
 * NegativeStart, ClipRangeOverflow, ClipKindMismatch, duplicate effect IDs, media and source range
 * checks, then track ClipOverlap issues, then markers in vector order), followed by compound cycles
 * (find_compound_cycles reports ONE cycle per back edge of the depth-first search (parallel edges
 * count once); it does not enumerate every possible simple cycle; after one cycle is fixed,
 * validating again may reveal another), and finally clip links in link ID map order.
 *
 * @param project The project document to inspect.
 * @return A list of all detected validation issues (empty if document is valid).
 *
 * @note Thread safety: Thread-safe (pure inspection function).
 */
[[nodiscard]] std::vector<ValidationIssue> collect_issues(const Project& project);

/**
 * @brief Validates a project document and returns a Status.
 *
 * Returns OK Status if no issues are detected.
 * Returns ErrorCode::InvalidArgument if any issues exist, formatting the message as:
 * "<N> validation issue(s); first: <code> at <path>: <message>".
 *
 * @param project The project document to inspect.
 * @return Success status, or ErrorCode::InvalidArgument describing the first failure.
 *
 * @note Thread safety: Thread-safe (pure inspection function).
 */
[[nodiscard]] core::Status validate(const Project& project);

}  // namespace nxtcut::model
