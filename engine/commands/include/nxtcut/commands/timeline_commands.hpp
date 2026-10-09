#pragma once

#include <nxtcut/commands/change_set.hpp>
#include <nxtcut/commands/timeline_edit_types.hpp>
#include <nxtcut/core/result.hpp>
#include <nxtcut/core/uuid.hpp>
#include <nxtcut/model/ids.hpp>
#include <nxtcut/model/project.hpp>
#include <nxtcut/model/time_coords.hpp>

#include <string>
#include <vector>

namespace nxtcut::commands {

// ============================================================================
// Part 1: Placement, Movement, Removal, and Splitting
// ============================================================================

/**
 * @brief Places clips at a specified timeline time without moving existing material.
 *
 * @note Thread safety: Command is a plain value; build() is const and reentrant.
 */
struct AddClips {
    model::SequenceId sequence;
    model::TimelineTime at;
    std::vector<PlacedClip> clips;

    [[nodiscard]] std::string label() const { return "Add Clips"; }
    [[nodiscard]] core::Result<ChangeSet> build(const model::Project& project,
                                                core::UuidGenerator& ids) const;
};

/**
 * @brief Inserts clips at a specified timeline position, splitting straddling clips and
 * shifting later material.
 *
 * @note Thread safety: Command is a plain value; build() is const and reentrant.
 */
struct InsertClips {
    model::SequenceId sequence;
    model::TimelineTime at;
    std::vector<PlacedClip> clips;
    RippleScope scope{RippleScope::AllUnlockedTracks};

    [[nodiscard]] std::string label() const { return "Insert Clips"; }
    [[nodiscard]] core::Result<ChangeSet> build(const model::Project& project,
                                                core::UuidGenerator& ids) const;
};

/**
 * @brief Places clips at a specified timeline position, overwriting whatever existing material
 * they cover on their destination tracks.
 *
 * @note Thread safety: Command is a plain value; build() is const and reentrant.
 */
struct OverwriteClips {
    model::SequenceId sequence;
    model::TimelineTime at;
    std::vector<PlacedClip> clips;

    [[nodiscard]] std::string label() const { return "Overwrite Clips"; }
    [[nodiscard]] core::Result<ChangeSet> build(const model::Project& project,
                                                core::UuidGenerator& ids) const;
};

/**
 * @brief Moves existing clips to new start positions and/or tracks.
 *
 * @note Thread safety: Command is a plain value; build() is const and reentrant.
 */
struct MoveClips {
    model::SequenceId sequence;
    std::vector<ClipMove> moves;
    bool ignore_links{false};

    [[nodiscard]] std::string label() const { return "Move Clips"; }
    [[nodiscard]] core::Result<ChangeSet> build(const model::Project& project,
                                                core::UuidGenerator& ids) const;
};

/**
 * @brief Lift deletes clips from the timeline, leaving gaps where they were.
 *
 * @note Thread safety: Command is a plain value; build() is const and reentrant.
 */
struct DeleteClips {
    model::SequenceId sequence;
    std::vector<model::ClipId> clip_ids;
    bool ignore_links{false};

    [[nodiscard]] std::string label() const { return "Delete Clips"; }
    [[nodiscard]] core::Result<ChangeSet> build(const model::Project& project,
                                                core::UuidGenerator& ids) const;
};

/**
 * @brief Splits a clip into two separate clips at a specified timeline instant.
 *
 * @note Thread safety: Command is a plain value; build() is const and reentrant.
 */
struct SplitClip {
    model::SequenceId sequence;
    model::ClipId clip;
    model::TimelineTime at;
    bool ignore_links{false};

    [[nodiscard]] std::string label() const { return "Split Clip"; }
    [[nodiscard]] core::Result<ChangeSet> build(const model::Project& project,
                                                core::UuidGenerator& ids) const;
};

// ============================================================================
// Part 2: Trimming, Ripple Deletion, Gap Closing, Joining, and Linking
// ============================================================================

/**
 * @brief Trims the in-point or out-point of a clip, with optional ripple shifting.
 *
 * @note Thread safety: Command is a plain value; build() is const and reentrant.
 */
struct TrimClip {
    model::SequenceId sequence;
    model::ClipId clip;
    TrimEdge edge{TrimEdge::Tail};
    model::TimelineTime new_edge;
    bool ripple{false};
    RippleScope scope{RippleScope::AllUnlockedTracks};
    bool ignore_links{false};

    [[nodiscard]] std::string label() const { return "Trim Clip"; }
    [[nodiscard]] core::Result<ChangeSet> build(const model::Project& project,
                                                core::UuidGenerator& ids) const;
};

/**
 * @brief Deletes clips and closes the removed time intervals across scope tracks.
 *
 * @note Thread safety: Command is a plain value; build() is const and reentrant.
 */
struct RippleDeleteClips {
    model::SequenceId sequence;
    std::vector<model::ClipId> clip_ids;
    RippleScope scope{RippleScope::AllUnlockedTracks};
    bool ignore_links{false};

    [[nodiscard]] std::string label() const { return "Ripple Delete Clips"; }
    [[nodiscard]] core::Result<ChangeSet> build(const model::Project& project,
                                                core::UuidGenerator& ids) const;
};

/**
 * @brief Closes an empty gap on a track by shifting subsequent material earlier.
 *
 * @note Thread safety: Command is a plain value; build() is const and reentrant.
 */
struct CloseGap {
    model::SequenceId sequence;
    model::TrackId track;
    model::TimelineTime at;
    RippleScope scope{RippleScope::AllUnlockedTracks};

    [[nodiscard]] std::string label() const { return "Close Gap"; }
    [[nodiscard]] core::Result<ChangeSet> build(const model::Project& project,
                                                core::UuidGenerator& ids) const;
};

/**
 * @brief Joins adjacent split clips back into a single contiguous clip.
 *
 * @note Thread safety: Command is a plain value; build() is const and reentrant.
 */
struct JoinClips {
    model::SequenceId sequence;
    std::vector<JoinPair> pairs;

    [[nodiscard]] std::string label() const { return "Join Clips"; }
    [[nodiscard]] core::Result<ChangeSet> build(const model::Project& project,
                                                core::UuidGenerator& ids) const;
};

/**
 * @brief Links multiple unlinked clips together with a fresh LinkId.
 *
 * @note Thread safety: Command is a plain value; build() is const and reentrant.
 */
struct LinkClips {
    model::SequenceId sequence;
    std::vector<model::ClipId> clip_ids;

    [[nodiscard]] std::string label() const { return "Link Clips"; }
    [[nodiscard]] core::Result<ChangeSet> build(const model::Project& project,
                                                core::UuidGenerator& ids) const;
};

/**
 * @brief Removes link grouping from specified clips.
 *
 * @note Thread safety: Command is a plain value; build() is const and reentrant.
 */
struct UnlinkClips {
    model::SequenceId sequence;
    std::vector<model::ClipId> clip_ids;
    bool ignore_links{false};

    [[nodiscard]] std::string label() const { return "Unlink Clips"; }
    [[nodiscard]] core::Result<ChangeSet> build(const model::Project& project,
                                                core::UuidGenerator& ids) const;
};

}  // namespace nxtcut::commands
