#include <nxtcut/core/error.hpp>
#include <nxtcut/core/mul_div.hpp>
#include <nxtcut/model/checked_arithmetic.hpp>
#include <nxtcut/model/clip.hpp>

#include <cstdint>
#include <string_view>
#include <type_traits>
#include <variant>

namespace nxtcut::model {

std::string_view to_string(ClipKind kind) noexcept {
    switch (kind) {
        case ClipKind::Video:
            return "Video";
        case ClipKind::Audio:
            return "Audio";
        case ClipKind::Image:
            return "Image";
        case ClipKind::Text:
            return "Text";
        case ClipKind::Compound:
            return "Compound";
    }
    return "Unknown";
}

ClipKind kind_of(const ClipContent& content) noexcept {
    return std::visit(
        [](const auto& c) -> ClipKind {
            using T = std::decay_t<decltype(c)>;
            if constexpr (std::is_same_v<T, VideoContent>) {
                return ClipKind::Video;
            } else if constexpr (std::is_same_v<T, AudioContent>) {
                return ClipKind::Audio;
            } else if constexpr (std::is_same_v<T, ImageContent>) {
                return ClipKind::Image;
            } else if constexpr (std::is_same_v<T, TextContent>) {
                return ClipKind::Text;
            } else if constexpr (std::is_same_v<T, CompoundContent>) {
                return ClipKind::Compound;
            } else {
                static_assert(sizeof(T) == 0, "unhandled ClipContent alternative");
            }
        },
        content);
}

core::Result<TimelineTime> clip_end(const Clip& clip) {
    const auto end_ticks = detail::checked_add(clip.start.ticks(), clip.duration.ticks());
    if (!end_ticks.has_value()) {
        return core::make_error(core::ErrorCode::Overflow, "clip_end arithmetic overflow");
    }
    return TimelineTime::from_ticks(*end_ticks);
}

core::Result<ClipTime> timeline_to_clip(const Clip& clip, TimelineTime t) {
    const auto end_res = clip_end(clip);
    if (!end_res.has_value()) {
        return tl::unexpected(end_res.error());
    }

    if (t < clip.start || t >= end_res.value()) {
        return core::make_error(core::ErrorCode::OutOfRange,
                                "timeline time outside clip half-open interval");
    }

    const auto diff = detail::checked_sub(t.ticks(), clip.start.ticks());
    if (!diff.has_value()) {
        return core::make_error(core::ErrorCode::Overflow, "timeline_to_clip arithmetic overflow");
    }
    return ClipTime::from_ticks(*diff);
}

core::Result<TimelineTime> clip_to_timeline(const Clip& clip, ClipTime c) {
    if (c.ticks() < 0 || c.ticks() > clip.duration.ticks()) {
        return core::make_error(core::ErrorCode::OutOfRange,
                                "clip time outside clip duration bounds");
    }

    const auto t_ticks = detail::checked_add(clip.start.ticks(), c.ticks());
    if (!t_ticks.has_value()) {
        return core::make_error(core::ErrorCode::Overflow, "clip_to_timeline arithmetic overflow");
    }
    return TimelineTime::from_ticks(*t_ticks);
}

core::Result<SourceTime> clip_to_source(const Clip& clip, ClipTime c) {
    if (c.ticks() < 0 || c.ticks() > clip.duration.ticks()) {
        return core::make_error(core::ErrorCode::OutOfRange,
                                "clip time outside clip duration bounds");
    }

    const auto scaled_res = core::mul_div(c.ticks(), clip.speed.numerator(),
                                          clip.speed.denominator(), core::RoundingMode::Floor);
    if (!scaled_res.has_value()) {
        return tl::unexpected(scaled_res.error());
    }

    const auto s_ticks = detail::checked_add(clip.source_in.ticks(), scaled_res.value());
    if (!s_ticks.has_value()) {
        return core::make_error(core::ErrorCode::Overflow, "clip_to_source arithmetic overflow");
    }
    return SourceTime::from_ticks(*s_ticks);
}

core::Result<SourceTime> timeline_to_source(const Clip& clip, TimelineTime t) {
    const auto clip_res = timeline_to_clip(clip, t);
    if (!clip_res.has_value()) {
        return tl::unexpected(clip_res.error());
    }
    return clip_to_source(clip, clip_res.value());
}

core::Result<ClipTime> source_to_clip(const Clip& clip, SourceTime s) {
    if (s < clip.source_in) {
        return core::make_error(core::ErrorCode::OutOfRange,
                                "source time earlier than clip source_in");
    }

    const auto diff = detail::checked_sub(s.ticks(), clip.source_in.ticks());
    if (!diff.has_value()) {
        return core::make_error(core::ErrorCode::Overflow, "source_to_clip arithmetic overflow");
    }

    const auto scaled_res = core::mul_div(*diff, clip.speed.denominator(), clip.speed.numerator(),
                                          core::RoundingMode::Floor);
    if (!scaled_res.has_value()) {
        return tl::unexpected(scaled_res.error());
    }

    if (scaled_res.value() > clip.duration.ticks()) {
        return core::make_error(core::ErrorCode::OutOfRange,
                                "source time maps beyond clip duration");
    }
    return ClipTime::from_ticks(scaled_res.value());
}

core::Result<core::Duration> source_span(const Clip& clip) {
    const auto scaled_res = core::mul_div(clip.duration.ticks(), clip.speed.numerator(),
                                          clip.speed.denominator(), core::RoundingMode::Ceil);
    if (!scaled_res.has_value()) {
        return tl::unexpected(scaled_res.error());
    }
    return core::Duration::from_ticks(scaled_res.value());
}

}  // namespace nxtcut::model
