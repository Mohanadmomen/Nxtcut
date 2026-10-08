#include <nxtcut/model/sequence.hpp>

#include <algorithm>

namespace nxtcut::model {

core::Result<core::Duration> sequence_duration(const Sequence& sequence) {
    std::int64_t max_end_ticks = 0;
    for (const auto& track : sequence.tracks) {
        for (const auto& clip : track.clips) {
            const auto end_res = clip_end(clip);
            if (!end_res.has_value()) {
                return tl::unexpected(end_res.error());
            }
            if (end_res.value().ticks() > max_end_ticks) {
                max_end_ticks = end_res.value().ticks();
            }
        }
    }
    return core::Duration::from_ticks(max_end_ticks);
}

const Track* find_track(const Sequence& sequence, TrackId id) noexcept {
    for (const auto& track : sequence.tracks) {
        if (track.id == id) {
            return &track;
        }
    }
    return nullptr;
}

const Clip* find_clip(const Sequence& sequence, ClipId id) noexcept {
    for (const auto& track : sequence.tracks) {
        for (const auto& clip : track.clips) {
            if (clip.id == id) {
                return &clip;
            }
        }
    }
    return nullptr;
}

}  // namespace nxtcut::model
