#include <nxtcut/commands/marker_commands.hpp>

#include <algorithm>
#include <utility>

namespace nxtcut::commands {

core::Result<ChangeSet> AddMarker::build(const model::Project& project,
                                         core::UuidGenerator& ids) const {
    if (time.ticks() < 0) {
        return core::make_error(core::ErrorCode::InvalidArgument,
                                "marker time must be non-negative");
    }

    auto seq_it = project.sequences.find(sequence);
    if (seq_it == project.sequences.end()) {
        return core::make_error(core::ErrorCode::NotFound, "sequence not found");
    }

    const model::MarkerId new_id = model::generate_id<model::MarkerId>(ids);
    model::Marker marker;
    marker.id = new_id;
    marker.time = time;
    marker.label = text;
    marker.color = color;

    ChangeSet cs;
    cs.label = label();
    cs.changes.push_back(MarkerChange{sequence, new_id, std::nullopt, std::move(marker)});
    return cs;
}

core::Result<ChangeSet> RemoveMarker::build(const model::Project& project,
                                            core::UuidGenerator& ids) const {
    static_cast<void>(ids);
    auto seq_it = project.sequences.find(sequence);
    if (seq_it == project.sequences.end()) {
        return core::make_error(core::ErrorCode::NotFound, "sequence not found");
    }

    const auto& markers = seq_it->second.markers;
    auto marker_it = std::find_if(markers.begin(), markers.end(),
                                  [this](const model::Marker& m) { return m.id == marker; });
    if (marker_it == markers.end()) {
        return core::make_error(core::ErrorCode::NotFound, "marker not found");
    }

    ChangeSet cs;
    cs.label = label();
    cs.changes.push_back(MarkerChange{sequence, marker, *marker_it, std::nullopt});
    return cs;
}

}  // namespace nxtcut::commands
