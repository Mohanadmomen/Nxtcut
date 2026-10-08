#pragma once

#include <nxtcut/model/ids.hpp>
#include <nxtcut/model/media.hpp>
#include <nxtcut/model/sequence.hpp>

#include <map>
#include <string>

namespace nxtcut::model {

/**
 * @brief Represents an entire editing project document containing assets and sequences.
 *
 * Invariant: Every map key in `media` and `sequences` must strictly match the `id`
 * of the associated entity value.
 *
 * @note Thread safety: plain data, no internal synchronization; concurrent const access is safe;
 * copies are independent.
 */
struct Project {
    ProjectId id;
    std::string name;
    std::map<MediaId, MediaAsset> media;
    std::map<SequenceId, Sequence> sequences;
    SequenceId main_sequence;
};

/**
 * @brief Looks up a MediaAsset in the project by its MediaId.
 *
 * @param project The project to search.
 * @param id The target media identifier.
 * @return Non-owning pointer to the MediaAsset, or nullptr if absent.
 *         Pointer remains valid until project.media is modified.
 */
[[nodiscard]] const MediaAsset* find_media(const Project& project, MediaId id) noexcept;

/**
 * @brief Looks up a Sequence in the project by its SequenceId.
 *
 * @param project The project to search.
 * @param id The target sequence identifier.
 * @return Non-owning pointer to the Sequence, or nullptr if absent.
 *         Pointer remains valid until project.sequences is modified.
 */
[[nodiscard]] const Sequence* find_sequence(const Project& project, SequenceId id) noexcept;

}  // namespace nxtcut::model
