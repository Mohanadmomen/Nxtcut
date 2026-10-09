#include <nxtcut/commands/apply.hpp>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace nxtcut::commands {

namespace {

[[nodiscard]] bool clip_canonical_less(const model::Clip& a, const model::Clip& b) noexcept {
    if (a.start.ticks() != b.start.ticks()) {
        return a.start.ticks() < b.start.ticks();
    }
    return a.id < b.id;
}

[[nodiscard]] bool marker_canonical_less(const model::Marker& a, const model::Marker& b) noexcept {
    if (a.time.ticks() != b.time.ticks()) {
        return a.time.ticks() < b.time.ticks();
    }
    return a.id < b.id;
}

core::Status apply_project_props(model::Project& project, const ProjectPropertiesChange& c) {
    project.name = c.after.name;
    project.main_sequence = c.after.main_sequence;
    return core::Status{};
}

core::Status apply_media(model::Project& project, const MediaChange& c) {
    if (!c.before.has_value() && !c.after.has_value()) {
        return core::make_error(core::ErrorCode::Internal,
                                "invalid media change: neither side present");
    }

    if (!c.before.has_value() && c.after.has_value()) {
        if (project.media.contains(c.id)) {
            return core::make_error(core::ErrorCode::AlreadyExists, "media asset already exists");
        }
        project.media[c.id] = *c.after;
        return core::Status{};
    }

    auto it = project.media.find(c.id);
    if (it == project.media.end()) {
        return core::make_error(core::ErrorCode::NotFound, "media asset not found");
    }

    if (c.before.has_value() && !c.after.has_value()) {
        project.media.erase(it);
    } else {
        it->second = *c.after;
    }
    return core::Status{};
}

core::Status apply_sequence(model::Project& project, const SequenceChange& c) {
    if ((c.before.has_value() && c.after.has_value()) ||
        (!c.before.has_value() && !c.after.has_value())) {
        return core::make_error(core::ErrorCode::Internal,
                                "invalid sequence change: lifecycle requires exactly one side");
    }

    if (c.after.has_value()) {
        if (project.sequences.contains(c.id)) {
            return core::make_error(core::ErrorCode::AlreadyExists, "sequence already exists");
        }
        auto& seq = project.sequences[c.id] = *c.after;
        for (auto& track : seq.tracks) {
            std::stable_sort(track.clips.begin(), track.clips.end(), clip_canonical_less);
        }
        std::stable_sort(seq.markers.begin(), seq.markers.end(), marker_canonical_less);
        return core::Status{};
    }

    auto it = project.sequences.find(c.id);
    if (it == project.sequences.end()) {
        return core::make_error(core::ErrorCode::NotFound, "sequence not found");
    }
    project.sequences.erase(it);
    return core::Status{};
}

core::Status apply_sequence_settings(model::Project& project, const SequenceSettingsChange& c) {
    auto it = project.sequences.find(c.id);
    if (it == project.sequences.end()) {
        return core::make_error(core::ErrorCode::NotFound, "sequence not found");
    }
    it->second.name = c.after.name;
    it->second.frame_rate = c.after.frame_rate;
    it->second.canvas = c.after.canvas;
    it->second.sample_rate = c.after.sample_rate;
    it->second.background = c.after.background;
    return core::Status{};
}

core::Status apply_track(model::Project& project, const TrackChange& c) {
    if ((c.before.has_value() && c.after.has_value()) ||
        (!c.before.has_value() && !c.after.has_value())) {
        return core::make_error(core::ErrorCode::Internal,
                                "invalid track change: lifecycle requires exactly one side");
    }

    auto seq_it = project.sequences.find(c.sequence);
    if (seq_it == project.sequences.end()) {
        return core::make_error(core::ErrorCode::NotFound, "sequence not found");
    }

    auto& tracks = seq_it->second.tracks;
    if (c.after.has_value()) {
        const bool exists = std::any_of(tracks.begin(), tracks.end(),
                                        [&c](const model::Track& t) { return t.id == c.id; });
        if (exists) {
            return core::make_error(core::ErrorCode::AlreadyExists, "track already exists");
        }
        const auto& slot = *c.after;
        if (slot.index > tracks.size()) {
            return core::make_error(core::ErrorCode::OutOfRange, "track slot index out of range");
        }
        auto insert_it =
            tracks.insert(tracks.begin() + static_cast<std::ptrdiff_t>(slot.index), slot.track);
        std::stable_sort(insert_it->clips.begin(), insert_it->clips.end(), clip_canonical_less);
        return core::Status{};
    }

    auto track_it = std::find_if(tracks.begin(), tracks.end(),
                                 [&c](const model::Track& t) { return t.id == c.id; });
    if (track_it == tracks.end()) {
        return core::make_error(core::ErrorCode::NotFound, "track not found");
    }
    tracks.erase(track_it);
    return core::Status{};
}

core::Status apply_track_move(model::Project& project, const TrackMove& c) {
    auto seq_it = project.sequences.find(c.sequence);
    if (seq_it == project.sequences.end()) {
        return core::make_error(core::ErrorCode::NotFound, "sequence not found");
    }

    auto& tracks = seq_it->second.tracks;
    if (c.from_index >= tracks.size() || c.to_index >= tracks.size()) {
        return core::make_error(core::ErrorCode::OutOfRange, "track move index out of range");
    }
    if (tracks[c.from_index].id != c.id) {
        return core::make_error(core::ErrorCode::NotFound, "track not found at from_index");
    }

    if (c.from_index < c.to_index) {
        std::rotate(tracks.begin() + static_cast<std::ptrdiff_t>(c.from_index),
                    tracks.begin() + static_cast<std::ptrdiff_t>(c.from_index) + 1,
                    tracks.begin() + static_cast<std::ptrdiff_t>(c.to_index) + 1);
    } else if (c.from_index > c.to_index) {
        std::rotate(tracks.begin() + static_cast<std::ptrdiff_t>(c.to_index),
                    tracks.begin() + static_cast<std::ptrdiff_t>(c.from_index),
                    tracks.begin() + static_cast<std::ptrdiff_t>(c.from_index) + 1);
    }
    return core::Status{};
}

core::Status apply_track_properties(model::Project& project, const TrackPropertiesChange& c) {
    auto seq_it = project.sequences.find(c.sequence);
    if (seq_it == project.sequences.end()) {
        return core::make_error(core::ErrorCode::NotFound, "sequence not found");
    }

    auto& tracks = seq_it->second.tracks;
    auto track_it = std::find_if(tracks.begin(), tracks.end(),
                                 [&c](const model::Track& t) { return t.id == c.id; });
    if (track_it == tracks.end()) {
        return core::make_error(core::ErrorCode::NotFound, "track not found");
    }

    track_it->name = c.after.name;
    track_it->enabled = c.after.enabled;
    track_it->locked = c.after.locked;
    return core::Status{};
}

core::Status apply_clip(model::Project& project, const ClipChange& c) {
    if (!c.before.has_value() && !c.after.has_value()) {
        return core::make_error(core::ErrorCode::Internal,
                                "invalid clip change: neither side present");
    }

    auto seq_it = project.sequences.find(c.sequence);
    if (seq_it == project.sequences.end()) {
        return core::make_error(core::ErrorCode::NotFound, "sequence not found");
    }

    auto& tracks = seq_it->second.tracks;
    auto track_it = std::find_if(tracks.begin(), tracks.end(),
                                 [&c](const model::Track& t) { return t.id == c.track; });
    if (track_it == tracks.end()) {
        return core::make_error(core::ErrorCode::NotFound, "track not found");
    }

    auto& clips = track_it->clips;
    auto clip_it = std::find_if(clips.begin(), clips.end(),
                                [&c](const model::Clip& cl) { return cl.id == c.id; });

    if (c.before.has_value()) {
        if (clip_it == clips.end()) {
            return core::make_error(core::ErrorCode::NotFound, "clip not found");
        }
        clips.erase(clip_it);
    } else {
        if (clip_it != clips.end()) {
            return core::make_error(core::ErrorCode::AlreadyExists, "clip already exists");
        }
    }

    if (c.after.has_value()) {
        auto insert_pos =
            std::lower_bound(clips.begin(), clips.end(), *c.after, clip_canonical_less);
        clips.insert(insert_pos, *c.after);
    }
    return core::Status{};
}

core::Status apply_marker(model::Project& project, const MarkerChange& c) {
    if (!c.before.has_value() && !c.after.has_value()) {
        return core::make_error(core::ErrorCode::Internal,
                                "invalid marker change: neither side present");
    }

    auto seq_it = project.sequences.find(c.sequence);
    if (seq_it == project.sequences.end()) {
        return core::make_error(core::ErrorCode::NotFound, "sequence not found");
    }

    auto& markers = seq_it->second.markers;
    auto marker_it = std::find_if(markers.begin(), markers.end(),
                                  [&c](const model::Marker& m) { return m.id == c.id; });

    if (c.before.has_value()) {
        if (marker_it == markers.end()) {
            return core::make_error(core::ErrorCode::NotFound, "marker not found");
        }
        markers.erase(marker_it);
    } else {
        if (marker_it != markers.end()) {
            return core::make_error(core::ErrorCode::AlreadyExists, "marker already exists");
        }
    }

    if (c.after.has_value()) {
        auto insert_pos =
            std::lower_bound(markers.begin(), markers.end(), *c.after, marker_canonical_less);
        markers.insert(insert_pos, *c.after);
    }
    return core::Status{};
}

}  // namespace

void normalize(model::Project& project) {
    for (auto& [seq_id, seq] : project.sequences) {
        static_cast<void>(seq_id);
        for (auto& track : seq.tracks) {
            std::stable_sort(track.clips.begin(), track.clips.end(), clip_canonical_less);
        }
        std::stable_sort(seq.markers.begin(), seq.markers.end(), marker_canonical_less);
    }
}

core::Status apply(model::Project& project, const ChangeSet& change_set) {
    for (const auto& change : change_set.changes) {
        core::Status status = std::visit(
            [&project](const auto& c) -> core::Status {
                using T = std::decay_t<decltype(c)>;
                if constexpr (std::is_same_v<T, ProjectPropertiesChange>) {
                    return apply_project_props(project, c);
                } else if constexpr (std::is_same_v<T, MediaChange>) {
                    return apply_media(project, c);
                } else if constexpr (std::is_same_v<T, SequenceChange>) {
                    return apply_sequence(project, c);
                } else if constexpr (std::is_same_v<T, SequenceSettingsChange>) {
                    return apply_sequence_settings(project, c);
                } else if constexpr (std::is_same_v<T, TrackChange>) {
                    return apply_track(project, c);
                } else if constexpr (std::is_same_v<T, TrackMove>) {
                    return apply_track_move(project, c);
                } else if constexpr (std::is_same_v<T, TrackPropertiesChange>) {
                    return apply_track_properties(project, c);
                } else if constexpr (std::is_same_v<T, ClipChange>) {
                    return apply_clip(project, c);
                } else if constexpr (std::is_same_v<T, MarkerChange>) {
                    return apply_marker(project, c);
                } else {
                    static_assert(sizeof(T) == 0, "unhandled Change alternative");
                }
            },
            change);

        if (!status.has_value()) {
            return status;
        }
    }
    return core::Status{};
}

}  // namespace nxtcut::commands
