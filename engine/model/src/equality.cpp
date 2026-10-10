#include <nxtcut/keyframes/keyframe_track.hpp>
#include <nxtcut/model/equality.hpp>

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <variant>

namespace nxtcut::model {

namespace {

[[nodiscard]] bool bit_identical_double(double a, double b) noexcept {
    return std::bit_cast<std::uint64_t>(a) == std::bit_cast<std::uint64_t>(b);
}

[[nodiscard]] bool bit_identical_float(float a, float b) noexcept {
    return std::bit_cast<std::uint32_t>(a) == std::bit_cast<std::uint32_t>(b);
}

template <keyframes::Animatable T>
[[nodiscard]] bool identical_tracks(const keyframes::KeyframeTrack<T>* a,
                                    const keyframes::KeyframeTrack<T>* b) noexcept {
    if (a == nullptr && b == nullptr) {
        return true;
    }
    if (a == nullptr || b == nullptr) {
        return false;
    }
    if (a == b) {
        return true;
    }
    return keyframes::identical(*a, *b);
}

}  // namespace

bool identical(const core::Color& a, const core::Color& b) noexcept {
    return bit_identical_float(a.r, b.r) && bit_identical_float(a.g, b.g) &&
           bit_identical_float(a.b, b.b) && bit_identical_float(a.a, b.a);
}

bool identical(const Property<double>& a, const Property<double>& b) noexcept {
    if (!bit_identical_double(a.constant_value(), b.constant_value())) {
        return false;
    }
    return identical_tracks(a.keyframes(), b.keyframes());
}

bool identical(const Property<bool>& a, const Property<bool>& b) noexcept {
    return a.constant_value() == b.constant_value();
}

bool identical(const Property<std::string>& a, const Property<std::string>& b) noexcept {
    return a.constant_value() == b.constant_value();
}

bool identical(const Property<core::Color>& a, const Property<core::Color>& b) noexcept {
    if (!identical(a.constant_value(), b.constant_value())) {
        return false;
    }
    return identical_tracks(a.keyframes(), b.keyframes());
}

bool identical(const VideoStreamInfo& a, const VideoStreamInfo& b) noexcept {
    return a.size == b.size && a.frame_rate == b.frame_rate;
}

bool identical(const AudioStreamInfo& a, const AudioStreamInfo& b) noexcept {
    return a.sample_rate == b.sample_rate && a.channels == b.channels;
}

bool identical(const TransformProps& a, const TransformProps& b) noexcept {
    return identical(a.position_x, b.position_x) && identical(a.position_y, b.position_y) &&
           identical(a.scale_x, b.scale_x) && identical(a.scale_y, b.scale_y) &&
           identical(a.rotation_degrees, b.rotation_degrees) && identical(a.anchor_x, b.anchor_x) &&
           identical(a.anchor_y, b.anchor_y) && identical(a.opacity, b.opacity) &&
           identical(a.crop_left, b.crop_left) && identical(a.crop_right, b.crop_right) &&
           identical(a.crop_top, b.crop_top) && identical(a.crop_bottom, b.crop_bottom);
}

bool identical(const EffectParam& a, const EffectParam& b) noexcept {
    if (a.index() != b.index()) {
        return false;
    }
    return std::visit(
        [&b](const auto& val_a) -> bool {
            using T = std::decay_t<decltype(val_a)>;
            const auto* val_b = std::get_if<T>(&b);
            if (val_b == nullptr) {
                return false;
            }
            return identical(val_a, *val_b);
        },
        a);
}

bool identical(const EffectInstance& a, const EffectInstance& b) noexcept {
    if (a.id != b.id || a.effect_type != b.effect_type || a.enabled != b.enabled ||
        a.params.size() != b.params.size()) {
        return false;
    }
    auto it_a = a.params.begin();
    auto it_b = b.params.begin();
    while (it_a != a.params.end()) {
        if (it_a->first != it_b->first || !identical(it_a->second, it_b->second)) {
            return false;
        }
        ++it_a;
        ++it_b;
    }
    return true;
}

bool identical(const MediaAsset& a, const MediaAsset& b) noexcept {
    if (a.id != b.id || a.name != b.name || a.path != b.path || a.kind != b.kind ||
        a.duration.ticks() != b.duration.ticks() || a.video.has_value() != b.video.has_value() ||
        a.audio.has_value() != b.audio.has_value()) {
        return false;
    }
    if (a.video.has_value() && !identical(*a.video, *b.video)) {
        return false;
    }
    if (a.audio.has_value() && !identical(*a.audio, *b.audio)) {
        return false;
    }
    return true;
}

bool identical(const ClipContent& a, const ClipContent& b) noexcept {
    if (a.index() != b.index()) {
        return false;
    }
    return std::visit(
        [&b](const auto& val_a) -> bool {
            using T = std::decay_t<decltype(val_a)>;
            const auto* val_b = std::get_if<T>(&b);
            if (val_b == nullptr) {
                return false;
            }
            if constexpr (std::is_same_v<T, VideoContent>) {
                return val_a.media == val_b->media;
            } else if constexpr (std::is_same_v<T, AudioContent>) {
                return val_a.media == val_b->media && identical(val_a.volume, val_b->volume) &&
                       val_a.fade_in.ticks() == val_b->fade_in.ticks() &&
                       val_a.fade_out.ticks() == val_b->fade_out.ticks();
            } else if constexpr (std::is_same_v<T, ImageContent>) {
                return val_a.media == val_b->media;
            } else if constexpr (std::is_same_v<T, TextContent>) {
                return val_a.text == val_b->text && val_a.font_family == val_b->font_family &&
                       identical(val_a.font_size_px, val_b->font_size_px) &&
                       identical(val_a.color, val_b->color);
            } else if constexpr (std::is_same_v<T, CompoundContent>) {
                return val_a.sequence == val_b->sequence;
            }
        },
        a);
}

bool identical(const Clip& a, const Clip& b) noexcept {
    if (a.id != b.id || a.name != b.name || a.start.ticks() != b.start.ticks() ||
        a.duration.ticks() != b.duration.ticks() || a.source_in.ticks() != b.source_in.ticks() ||
        a.speed != b.speed || a.enabled != b.enabled || a.link_id != b.link_id ||
        a.blend_mode != b.blend_mode || !identical(a.transform, b.transform) ||
        !identical(a.content, b.content) || a.effects.size() != b.effects.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.effects.size(); ++i) {
        if (!identical(a.effects[i], b.effects[i])) {
            return false;
        }
    }
    return true;
}

bool identical(const Marker& a, const Marker& b) noexcept {
    return a.id == b.id && a.time.ticks() == b.time.ticks() && a.label == b.label &&
           identical(a.color, b.color);
}

bool identical(const Track& a, const Track& b) noexcept {
    if (a.id != b.id || a.name != b.name || a.kind != b.kind || a.enabled != b.enabled ||
        a.locked != b.locked || a.clips.size() != b.clips.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.clips.size(); ++i) {
        if (!identical(a.clips[i], b.clips[i])) {
            return false;
        }
    }
    return true;
}

bool identical(const Sequence& a, const Sequence& b) noexcept {
    if (a.id != b.id || a.name != b.name || a.frame_rate != b.frame_rate || a.canvas != b.canvas ||
        a.sample_rate != b.sample_rate || !identical(a.background, b.background) ||
        a.tracks.size() != b.tracks.size() || a.markers.size() != b.markers.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.tracks.size(); ++i) {
        if (!identical(a.tracks[i], b.tracks[i])) {
            return false;
        }
    }
    for (std::size_t i = 0; i < a.markers.size(); ++i) {
        if (!identical(a.markers[i], b.markers[i])) {
            return false;
        }
    }
    return true;
}

bool identical(const Project& a, const Project& b) noexcept {
    if (a.id != b.id || a.name != b.name || a.main_sequence != b.main_sequence ||
        a.media.size() != b.media.size() || a.sequences.size() != b.sequences.size()) {
        return false;
    }
    auto it_media_a = a.media.begin();
    auto it_media_b = b.media.begin();
    while (it_media_a != a.media.end()) {
        if (it_media_a->first != it_media_b->first ||
            !identical(it_media_a->second, it_media_b->second)) {
            return false;
        }
        ++it_media_a;
        ++it_media_b;
    }
    auto it_seq_a = a.sequences.begin();
    auto it_seq_b = b.sequences.begin();
    while (it_seq_a != a.sequences.end()) {
        if (it_seq_a->first != it_seq_b->first || !identical(it_seq_a->second, it_seq_b->second)) {
            return false;
        }
        ++it_seq_a;
        ++it_seq_b;
    }
    return true;
}

}  // namespace nxtcut::model
