#pragma once

#include <nxtcut/core/color.hpp>
#include <nxtcut/core/frame_rate.hpp>
#include <nxtcut/core/geometry.hpp>
#include <nxtcut/core/result.hpp>
#include <nxtcut/core/time.hpp>
#include <nxtcut/model/ids.hpp>
#include <nxtcut/model/time_coords.hpp>
#include <nxtcut/model/track.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace nxtcut::model {

/**
 * @brief Represents a timeline marker placed at a specific timeline instant.
 *
 * @note Thread safety: plain data, no internal synchronization; concurrent const access is safe;
 * copies are independent.
 */
struct Marker {
    MarkerId id;
    TimelineTime time;
    std::string label;
    core::Color color{core::Color{1.0f, 1.0f, 1.0f, 1.0f}};
};

/**
 * @brief Represents a timeline sequence containing layered tracks and markers.
 *
 * Track ordering convention: Track index 0 represents the bottommost video layer
 * (composited first, below higher track indices).
 *
 * @note Thread safety: plain data, no internal synchronization; concurrent const access is safe;
 * copies are independent.
 */
struct Sequence {
    SequenceId id;
    std::string name;
    core::FrameRate frame_rate{core::frame_rates::k30};
    core::Size<std::int32_t> canvas{1920, 1080};
    core::SampleRate sample_rate{core::sample_rates::k48000};
    core::Color background{core::Color{0.0f, 0.0f, 0.0f, 1.0f}};
    std::vector<Track> tracks;
    std::vector<Marker> markers;
};

/**
 * @brief Computes the overall duration of a sequence.
 *
 * Defined as the latest clip_end across all clips across all tracks.
 * Returns Duration::zero() if the sequence contains no clips.
 * Propagates ErrorCode::Overflow if any clip_end calculation overflows.
 *
 * @param sequence The sequence to evaluate.
 * @return Total sequence duration, or ErrorCode::Overflow.
 */
[[nodiscard]] core::Result<core::Duration> sequence_duration(const Sequence& sequence);

/**
 * @brief Finds a track in a sequence by its TrackId.
 *
 * @param sequence The sequence to search.
 * @param id The target track identifier.
 * @return Non-owning pointer to the matching Track, or nullptr if not found.
 *         Pointer remains valid until sequence.tracks is modified.
 */
[[nodiscard]] const Track* find_track(const Sequence& sequence, TrackId id) noexcept;

/**
 * @brief Finds a clip anywhere in a sequence by its ClipId.
 *
 * @param sequence The sequence to search.
 * @param id The target clip identifier.
 * @return Non-owning pointer to the matching Clip, or nullptr if not found.
 *         Pointer remains valid until the containing track's clips are modified.
 */
[[nodiscard]] const Clip* find_clip(const Sequence& sequence, ClipId id) noexcept;

}  // namespace nxtcut::model
