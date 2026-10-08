#pragma once

#include <nxtcut/core/id.hpp>
#include <nxtcut/core/uuid.hpp>

namespace nxtcut::model {

/**
 * @brief Tag for identifying Project entities.
 */
struct ProjectTag {};

/**
 * @brief Tag for identifying Sequence entities.
 */
struct SequenceTag {};

/**
 * @brief Tag for identifying Track entities.
 */
struct TrackTag {};

/**
 * @brief Tag for identifying Clip entities.
 */
struct ClipTag {};

/**
 * @brief Tag for identifying MediaAsset entities.
 */
struct MediaTag {};

/**
 * @brief Tag for identifying EffectInstance entities.
 */
struct EffectTag {};

/**
 * @brief Tag for identifying Marker entities.
 */
struct MarkerTag {};

/**
 * @brief Tag for identifying linked clip relationships.
 */
struct LinkTag {};

/** @brief Strongly typed identifier for projects. */
using ProjectId = core::Id<ProjectTag>;

/** @brief Strongly typed identifier for sequences. */
using SequenceId = core::Id<SequenceTag>;

/** @brief Strongly typed identifier for tracks. */
using TrackId = core::Id<TrackTag>;

/** @brief Strongly typed identifier for clips. */
using ClipId = core::Id<ClipTag>;

/** @brief Strongly typed identifier for media assets. */
using MediaId = core::Id<MediaTag>;

/** @brief Strongly typed identifier for effects. */
using EffectId = core::Id<EffectTag>;

/** @brief Strongly typed identifier for markers. */
using MarkerId = core::Id<MarkerTag>;

/** @brief Strongly typed identifier for clip links. */
using LinkId = core::Id<LinkTag>;

/**
 * @brief Generates a new strongly typed entity identifier using the supplied UUID generator.
 *
 * @tparam IdT Strongly typed ID specialization (e.g. ClipId, TrackId).
 * @param generator Reference to a thread-safe UuidGenerator instance.
 * @return A newly generated IdT initialized with a version-4 UUID.
 *
 * @note Thread safety: Thread-safe if generator is thread-safe; generator provides internal
 * synchronization.
 */
template <class IdT>
[[nodiscard]] inline IdT generate_id(core::UuidGenerator& generator) {
    return IdT(generator.generate());
}

}  // namespace nxtcut::model
