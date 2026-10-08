#pragma once

#include <nxtcut/core/frame_rate.hpp>
#include <nxtcut/core/geometry.hpp>
#include <nxtcut/core/time.hpp>
#include <nxtcut/model/ids.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace nxtcut::model {

/**
 * @brief Discriminator for media asset content kinds.
 */
enum class MediaKind : std::uint8_t {
    Video,
    Audio,
    Image,
};

/**
 * @brief Converts a MediaKind enumerator to its string name.
 */
[[nodiscard]] std::string_view to_string(MediaKind kind) noexcept;

/**
 * @brief Metadata describing a video stream within a media asset.
 *
 * @note Thread safety: plain data, no internal synchronization; concurrent const access is safe;
 * copies are independent.
 */
struct VideoStreamInfo {
    core::Size<std::int32_t> size{};
    core::FrameRate frame_rate{core::frame_rates::k30};
};

/**
 * @brief Metadata describing an audio stream within a media asset.
 *
 * @note Thread safety: plain data, no internal synchronization; concurrent const access is safe;
 * copies are independent.
 */
struct AudioStreamInfo {
    core::SampleRate sample_rate{core::sample_rates::k48000};
    std::int32_t channels{2};
};

/**
 * @brief Represents an imported media asset referenced by timeline clips.
 *
 * Clips reference media solely by MediaId, never by filesystem path.
 * Filesystem paths reside exclusively within MediaAsset.
 * For Image assets, duration is zero and ignored by validation.
 *
 * @note Thread safety: plain data, no internal synchronization; concurrent const access is safe;
 * copies are independent.
 */
struct MediaAsset {
    MediaId id;
    std::string name;
    std::string path;
    MediaKind kind{MediaKind::Video};
    core::Duration duration{core::Duration::zero()};
    std::optional<VideoStreamInfo> video;
    std::optional<AudioStreamInfo> audio;
};

}  // namespace nxtcut::model
