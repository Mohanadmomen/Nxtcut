#pragma once

#include <nxtcut/model/clip.hpp>
#include <nxtcut/model/ids.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace nxtcut::model {

/**
 * @brief Discriminator for track media classifications.
 */
enum class TrackKind : std::uint8_t {
    Video,
    Audio,
};

/**
 * @brief Converts a TrackKind enumerator to its string name.
 */
[[nodiscard]] std::string_view to_string(TrackKind kind) noexcept;

/**
 * @brief Represents a timeline track containing an ordered or unordered collection of clips.
 *
 * Rules:
 * - Video, Image, Text, and Compound clips belong on Video tracks.
 * - Audio clips belong on Audio tracks.
 * - Clips are not required to be stored sorted; validation sorts internally.
 * - enabled: visible for video tracks, audible for audio tracks.
 *
 * @note Thread safety: plain data, no internal synchronization; concurrent const access is safe;
 * copies are independent.
 */
struct Track {
    TrackId id;
    std::string name;
    TrackKind kind{TrackKind::Video};
    bool enabled{true};
    bool locked{false};
    std::vector<Clip> clips;
};

}  // namespace nxtcut::model
