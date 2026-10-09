#include <nxtcut/commands/track_commands.hpp>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <utility>
#include <vector>

namespace nxtcut::commands {

core::Result<ChangeSet> AddTrack::build(const model::Project& project,
                                        core::UuidGenerator& ids) const {
    auto seq_it = project.sequences.find(sequence);
    if (seq_it == project.sequences.end()) {
        return core::make_error(core::ErrorCode::NotFound, "sequence not found");
    }

    const std::size_t target_idx = index.value_or(seq_it->second.tracks.size());
    if (target_idx > seq_it->second.tracks.size()) {
        return core::make_error(core::ErrorCode::OutOfRange, "track index out of range");
    }

    const model::TrackId new_id = model::generate_id<model::TrackId>(ids);
    model::Track new_track;
    new_track.id = new_id;
    new_track.name = name;
    new_track.kind = kind;
    new_track.enabled = true;
    new_track.locked = false;

    ChangeSet cs;
    cs.label = label();
    cs.changes.push_back(
        TrackChange{sequence, new_id, std::nullopt, TrackSlot{target_idx, std::move(new_track)}});
    return cs;
}

core::Result<ChangeSet> RemoveTrack::build(const model::Project& project,
                                           core::UuidGenerator& ids) const {
    static_cast<void>(ids);
    auto seq_it = project.sequences.find(sequence);
    if (seq_it == project.sequences.end()) {
        return core::make_error(core::ErrorCode::NotFound, "sequence not found");
    }

    const auto& tracks = seq_it->second.tracks;
    auto track_it = std::find_if(tracks.begin(), tracks.end(),
                                 [this](const model::Track& t) { return t.id == track; });
    if (track_it == tracks.end()) {
        return core::make_error(core::ErrorCode::NotFound, "track not found");
    }

    if (track_it->locked) {
        return core::make_error(core::ErrorCode::InvalidArgument, "cannot remove locked track");
    }

    const std::size_t track_idx = static_cast<std::size_t>(std::distance(tracks.begin(), track_it));

    ChangeSet cs;
    cs.label = label();

    // 1. Remove the track
    cs.changes.push_back(
        TrackChange{sequence, track, TrackSlot{track_idx, *track_it}, std::nullopt});

    // 2. Link cleanup: for every LinkId used on removed track,
    // if fewer than 2 clips with that link remain in sequence outside removed track,
    // clear link_id on those remaining clips.
    std::vector<model::LinkId> removed_links;
    for (const auto& clip : track_it->clips) {
        if (clip.link_id.has_value()) {
            if (std::find(removed_links.begin(), removed_links.end(), *clip.link_id) ==
                removed_links.end()) {
                removed_links.push_back(*clip.link_id);
            }
        }
    }

    for (const auto& link_id : removed_links) {
        std::vector<std::pair<model::TrackId, model::Clip>> remaining_with_link;
        for (const auto& other_track : tracks) {
            if (other_track.id == track) {
                continue;
            }
            for (const auto& clip : other_track.clips) {
                if (clip.link_id.has_value() && *clip.link_id == link_id) {
                    remaining_with_link.push_back({other_track.id, clip});
                }
            }
        }

        if (remaining_with_link.size() < 2) {
            for (const auto& [other_track_id, clip] : remaining_with_link) {
                model::Clip after_clip = clip;
                after_clip.link_id = std::nullopt;
                cs.changes.push_back(
                    ClipChange{sequence, other_track_id, clip.id, clip, after_clip});
            }
        }
    }

    return cs;
}

core::Result<ChangeSet> MoveTrack::build(const model::Project& project,
                                         core::UuidGenerator& ids) const {
    static_cast<void>(ids);
    auto seq_it = project.sequences.find(sequence);
    if (seq_it == project.sequences.end()) {
        return core::make_error(core::ErrorCode::NotFound, "sequence not found");
    }

    const auto& tracks = seq_it->second.tracks;
    auto track_it = std::find_if(tracks.begin(), tracks.end(),
                                 [this](const model::Track& t) { return t.id == track; });
    if (track_it == tracks.end()) {
        return core::make_error(core::ErrorCode::NotFound, "track not found");
    }

    if (new_index >= tracks.size()) {
        return core::make_error(core::ErrorCode::OutOfRange, "new track index out of range");
    }

    const std::size_t current_index =
        static_cast<std::size_t>(std::distance(tracks.begin(), track_it));
    if (current_index == new_index) {
        return ChangeSet{label(), {}};
    }

    ChangeSet cs;
    cs.label = label();
    cs.changes.push_back(TrackMove{sequence, track, current_index, new_index});
    return cs;
}

core::Result<ChangeSet> SetTrackProperties::build(const model::Project& project,
                                                  core::UuidGenerator& ids) const {
    static_cast<void>(ids);
    if (!name.has_value() && !enabled.has_value() && !locked.has_value()) {
        return core::make_error(core::ErrorCode::InvalidArgument,
                                "at least one property must be specified");
    }

    auto seq_it = project.sequences.find(sequence);
    if (seq_it == project.sequences.end()) {
        return core::make_error(core::ErrorCode::NotFound, "sequence not found");
    }

    const auto& tracks = seq_it->second.tracks;
    auto track_it = std::find_if(tracks.begin(), tracks.end(),
                                 [this](const model::Track& t) { return t.id == track; });
    if (track_it == tracks.end()) {
        return core::make_error(core::ErrorCode::NotFound, "track not found");
    }

    TrackProperties before{track_it->name, track_it->enabled, track_it->locked};
    TrackProperties after{name.value_or(before.name), enabled.value_or(before.enabled),
                          locked.value_or(before.locked)};

    if (after.name == before.name && after.enabled == before.enabled &&
        after.locked == before.locked) {
        return ChangeSet{label(), {}};
    }

    ChangeSet cs;
    cs.label = label();
    cs.changes.push_back(TrackPropertiesChange{sequence, track, before, after});
    return cs;
}

}  // namespace nxtcut::commands
