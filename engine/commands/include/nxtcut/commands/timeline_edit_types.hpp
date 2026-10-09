#pragma once

#include <nxtcut/model/clip.hpp>
#include <nxtcut/model/ids.hpp>
#include <nxtcut/model/time_coords.hpp>

#include <vector>

namespace nxtcut::commands {

/**
 * @brief Scope for timeline ripple operations.
 */
enum class RippleScope {
    AllUnlockedTracks,  ///< Ripple shifts all unlocked tracks in the sequence.
    EditedTracksOnly,    ///< Ripple shifts only the tracks containing edited/placed clips.
};

/**
 * @brief Specifies which edge of a clip is trimmed.
 */
enum class TrimEdge {
    Head,  ///< Trim the in-point (start) of the clip.
    Tail,  ///< Trim the out-point (end) of the clip.
};

/**
 * @brief Descriptor for placing a clip onto a specific track.
 *
 * The id, start, and link_id fields of the supplied clip are ignored:
 * fresh ClipIds are generated during command build(), the placement time
 * is supplied as a command parameter, and link_ids are coordinated across
 * placed clips.
 */
struct PlacedClip {
    model::TrackId track;
    model::Clip clip;
};

/**
 * @brief Descriptor for moving an existing clip to a new start position and/or track.
 */
struct ClipMove {
    model::ClipId clip;
    model::TimelineTime new_start;
    model::TrackId new_track;
};

/**
 * @brief Pair of adjacent clips to be joined back into a single contiguous clip.
 */
struct JoinPair {
    model::ClipId first;
    model::ClipId second;
};

}  // namespace nxtcut::commands
