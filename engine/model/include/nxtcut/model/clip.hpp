#pragma once

#include <nxtcut/core/color.hpp>
#include <nxtcut/core/result.hpp>
#include <nxtcut/core/time.hpp>
#include <nxtcut/model/blend_mode.hpp>
#include <nxtcut/model/effect.hpp>
#include <nxtcut/model/ids.hpp>
#include <nxtcut/model/property.hpp>
#include <nxtcut/model/speed.hpp>
#include <nxtcut/model/time_coords.hpp>
#include <nxtcut/model/transform.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace nxtcut::model {

/**
 * @brief Content data for a visual video clip referencing a media asset.
 *
 * @note Thread safety: plain data, no internal synchronization; concurrent const access is safe;
 * copies are independent.
 */
struct VideoContent {
    MediaId media;
};

/**
 * @brief Content data for an audio clip referencing a media asset.
 *
 * @note Thread safety: plain data, no internal synchronization; concurrent const access is safe;
 * copies are independent.
 */
struct AudioContent {
    MediaId media;
    Property<double> volume{1.0};
    core::Duration fade_in{core::Duration::zero()};
    core::Duration fade_out{core::Duration::zero()};
};

/**
 * @brief Content data for a static image clip referencing a media asset.
 *
 * @note Thread safety: plain data, no internal synchronization; concurrent const access is safe;
 * copies are independent.
 */
struct ImageContent {
    MediaId media;
};

/**
 * @brief Content data for a rendered text overlay clip.
 *
 * @note Thread safety: plain data, no internal synchronization; concurrent const access is safe;
 * copies are independent.
 */
struct TextContent {
    std::string text;
    std::string font_family{"sans-serif"};
    Property<double> font_size_px{48.0};
    Property<core::Color> color{core::Color{1.0f, 1.0f, 1.0f, 1.0f}};
};

/**
 * @brief Content data for a nested sequence (compound clip) referencing another sequence.
 *
 * @note Thread safety: plain data, no internal synchronization; concurrent const access is safe;
 * copies are independent.
 */
struct CompoundContent {
    SequenceId sequence;
};

/**
 * @brief Variant representing all supported clip content types.
 */
using ClipContent =
    std::variant<VideoContent, AudioContent, ImageContent, TextContent, CompoundContent>;

/**
 * @brief Discriminator for clip content kinds.
 */
enum class ClipKind : std::uint8_t {
    Video,
    Audio,
    Image,
    Text,
    Compound,
};

/**
 * @brief Converts a ClipKind enumerator to its string name.
 */
[[nodiscard]] std::string_view to_string(ClipKind kind) noexcept;

/**
 * @brief Determines the ClipKind discriminator for the given ClipContent variant.
 */
[[nodiscard]] ClipKind kind_of(const ClipContent& content) noexcept;

/**
 * @brief Represents a timeline clip entity placed on a track.
 *
 * Architecture and usage rules:
 * - A video clip has no audio of its own: its sound is a SEPARATE AudioContent clip on an audio
 * track sharing the same LinkId.
 * - transform and blend_mode are meaningful for visual clips and ignored for audio clips.
 * - For Image and Text clips source_in must be zero and speed must be 1/1.
 *
 * @note Thread safety: plain data, no internal synchronization; concurrent const access is safe;
 * copies are independent.
 */
struct Clip {
    ClipId id;
    std::string name;
    TimelineTime start;
    core::Duration duration{core::Duration::zero()};
    SourceTime source_in;
    Speed speed{Speed::normal()};
    bool enabled{true};
    std::optional<LinkId> link_id;
    TransformProps transform;
    BlendMode blend_mode{BlendMode::Normal};
    std::vector<EffectInstance> effects;
    ClipContent content;
};

/**
 * @brief Computes the exclusive end time of a clip on the timeline (start + duration).
 *
 * @param clip The clip to query.
 * @return Exclusive end TimelineTime, or ErrorCode::Overflow if arithmetic overflows.
 */
[[nodiscard]] core::Result<TimelineTime> clip_end(const Clip& clip);

/**
 * @brief Maps a timeline instant into the clip's local coordinate system.
 *
 * Valid only for start <= t < end (half-open interval [start, end)).
 *
 * @param clip The reference clip.
 * @param t Timeline instant to map.
 * @return Local ClipTime, or ErrorCode::OutOfRange if t falls outside [start, end),
 *         or ErrorCode::Overflow on arithmetic overflow.
 */
[[nodiscard]] core::Result<ClipTime> timeline_to_clip(const Clip& clip, TimelineTime t);

/**
 * @brief Maps a local clip offset back onto the sequence timeline.
 *
 * Valid for 0 <= c <= duration (end INCLUSIVE, enabling calculation of out-points).
 *
 * @param clip The reference clip.
 * @param c Local offset from clip start.
 * @return Timeline instant, or ErrorCode::OutOfRange if c is outside [0, duration],
 *         or ErrorCode::Overflow on arithmetic overflow.
 */
[[nodiscard]] core::Result<TimelineTime> clip_to_timeline(const Clip& clip, ClipTime c);

/**
 * @brief Maps a local clip offset into the source media coordinate system.
 *
 * Computed as: source_in + floor(c * numerator / denominator).
 * Valid for 0 <= c <= duration (end INCLUSIVE).
 * Round trips with source_to_clip are exact only when the rational division is exact.
 *
 * @param clip The reference clip.
 * @param c Local offset from clip start.
 * @return Source media instant, or ErrorCode::OutOfRange if c is outside [0, duration],
 *         or ErrorCode::Overflow on arithmetic overflow.
 */
[[nodiscard]] core::Result<SourceTime> clip_to_source(const Clip& clip, ClipTime c);

/**
 * @brief Maps a timeline instant directly into the source media coordinate system.
 *
 * Composition of timeline_to_clip followed by clip_to_source.
 *
 * @param clip The reference clip.
 * @param t Timeline instant to map.
 * @return Source media instant, or ErrorCode::OutOfRange if t falls outside [start, end),
 *         or ErrorCode::Overflow on arithmetic overflow.
 */
[[nodiscard]] core::Result<SourceTime> timeline_to_source(const Clip& clip, TimelineTime t);

/**
 * @brief Maps a source media instant back to a local clip offset.
 *
 * Computed as: floor((s - source_in) * denominator / numerator).
 * Valid only if s >= source_in and the resulting clip offset does not exceed duration.
 * Round trips with clip_to_source are exact only when the rational division is exact.
 *
 * @param clip The reference clip.
 * @param s Source media instant.
 * @return Local ClipTime, or ErrorCode::OutOfRange if s < source_in or result exceeds duration,
 *         or ErrorCode::Overflow on arithmetic overflow.
 */
[[nodiscard]] core::Result<ClipTime> source_to_clip(const Clip& clip, SourceTime s);

/**
 * @brief Computes the amount of source media duration consumed by the clip.
 *
 * Computed as: ceil(duration * numerator / denominator) (conservative ceiling rounding).
 *
 * @param clip The reference clip.
 * @return The source duration span, or ErrorCode::Overflow on arithmetic overflow.
 */
[[nodiscard]] core::Result<core::Duration> source_span(const Clip& clip);

}  // namespace nxtcut::model
