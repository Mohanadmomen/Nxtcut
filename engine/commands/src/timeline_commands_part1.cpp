#include <nxtcut/commands/timeline_commands.hpp>
#include "timeline_support.hpp"

#include <nxtcut/core/error.hpp>
#include <nxtcut/model/clip.hpp>
#include <nxtcut/model/compound_graph.hpp>
#include <nxtcut/model/ids.hpp>

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace nxtcut::commands {

// ============================================================================
// AddClips
// ============================================================================

core::Result<ChangeSet> AddClips::build(const model::Project& project,
                                        core::UuidGenerator& ids) const {
    if (clips.empty()) {
        return core::make_error(core::ErrorCode::InvalidArgument, "clips list cannot be empty");
    }

    auto st_res = detail::ScratchTimeline::create(project, sequence, ids);
    if (!st_res.has_value()) {
        return tl::unexpected(st_res.error());
    }
    auto& st = st_res.value();

    const auto snapped_at_res = st.snap_time(at);
    if (!snapped_at_res.has_value()) {
        return tl::unexpected(snapped_at_res.error());
    }
    const auto snapped_at = snapped_at_res.value();

    // Validate destination tracks and clip content
    for (const auto& pc : clips) {
        auto* trk = st.find_track(pc.track);
        if (trk == nullptr) {
            return core::make_error(core::ErrorCode::NotFound, "destination track not found");
        }
        if (trk->locked) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "cannot add clips to locked track");
        }
        if (!st.is_clip_compatible(trk->kind, pc.clip.content)) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "clip kind incompatible with track kind");
        }
        const auto snap_dur_res = st.snap_duration(pc.clip.duration);
        if (!snap_dur_res.has_value()) {
            return tl::unexpected(snap_dur_res.error());
        }
    }

    const std::optional<model::LinkId> common_link =
        (clips.size() > 1) ? std::optional(model::generate_id<model::LinkId>(ids)) : std::nullopt;

    for (const auto& pc : clips) {
        auto* trk = st.find_track(pc.track);
        const auto snap_dur = st.snap_duration(pc.clip.duration).value();

        model::Clip new_clip = pc.clip;
        new_clip.id = model::generate_id<model::ClipId>(ids);
        new_clip.start = snapped_at;
        new_clip.duration = snap_dur;
        new_clip.link_id = common_link;

        const auto val_status = st.validate_clip_media_and_source(new_clip);
        if (!val_status.has_value()) {
            return tl::unexpected(val_status.error());
        }

        trk->clips.push_back(std::move(new_clip));
        trk->touched = true;
    }

    const auto overlap_status = st.check_overlaps_on_touched_tracks();
    if (!overlap_status.has_value()) {
        return tl::unexpected(overlap_status.error());
    }

    const auto repair_status = st.repair_links();
    if (!repair_status.has_value()) {
        return tl::unexpected(repair_status.error());
    }

    return st.diff_to_changes(label());
}

// ============================================================================
// InsertClips
// ============================================================================

core::Result<ChangeSet> InsertClips::build(const model::Project& project,
                                           core::UuidGenerator& ids) const {
    if (clips.empty()) {
        return core::make_error(core::ErrorCode::InvalidArgument, "clips list cannot be empty");
    }

    auto st_res = detail::ScratchTimeline::create(project, sequence, ids);
    if (!st_res.has_value()) {
        return tl::unexpected(st_res.error());
    }
    auto& st = st_res.value();

    const auto snapped_at_res = st.snap_time(at);
    if (!snapped_at_res.has_value()) {
        return tl::unexpected(snapped_at_res.error());
    }
    const auto snapped_at = snapped_at_res.value();

    core::Duration delta = core::Duration::zero();
    std::unordered_set<model::TrackId> dest_track_ids;

    for (const auto& pc : clips) {
        auto* trk = st.find_track(pc.track);
        if (trk == nullptr) {
            return core::make_error(core::ErrorCode::NotFound, "destination track not found");
        }
        if (trk->locked) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "cannot insert clips into locked track");
        }
        if (!st.is_clip_compatible(trk->kind, pc.clip.content)) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "clip kind incompatible with track kind");
        }
        const auto snap_dur_res = st.snap_duration(pc.clip.duration);
        if (!snap_dur_res.has_value()) {
            return tl::unexpected(snap_dur_res.error());
        }
        if (snap_dur_res.value() > delta) {
            delta = snap_dur_res.value();
        }
        dest_track_ids.insert(pc.track);
    }

    // Determine scope tracks
    std::unordered_set<model::TrackId> scope_tracks;
    for (const auto& tid : dest_track_ids) {
        scope_tracks.insert(tid);
    }

    if (scope == RippleScope::AllUnlockedTracks) {
        for (const auto& trk : st.tracks()) {
            if (!trk.locked) {
                scope_tracks.insert(trk.id);
            }
        }
    } else {
        // EditedTracksOnly: check if any straddling clip on destination tracks has linked partners
        for (const auto& trk : st.tracks()) {
            if (!scope_tracks.contains(trk.id)) {
                continue;
            }
            for (const auto& cl : trk.clips) {
                const auto end_res = model::clip_end(cl);
                if (end_res.has_value() && cl.start < snapped_at && snapped_at < *end_res) {
                    if (cl.link_id.has_value()) {
                        for (const auto& other_trk : st.tracks()) {
                            for (const auto& other_cl : other_trk.clips) {
                                if (other_cl.link_id == cl.link_id) {
                                    scope_tracks.insert(other_trk.id);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Split straddling clips on scope tracks
    struct SplitEntry {
        model::TrackId track;
        model::Clip left;
        model::Clip right;
        std::optional<model::LinkId> orig_link;
    };
    std::vector<SplitEntry> splits;

    for (auto& trk : st.tracks()) {
        if (!scope_tracks.contains(trk.id)) {
            continue;
        }

        for (const auto& cl : trk.clips) {
            const auto end_res = model::clip_end(cl);
            if (!end_res.has_value()) {
                return tl::unexpected(end_res.error());
            }

            if (cl.start < snapped_at && snapped_at < *end_res) {
                if (trk.locked) {
                    return core::make_error(core::ErrorCode::InvalidArgument,
                                            "cannot split clip on locked track");
                }
                auto split_res = st.split_clip(cl, snapped_at);
                if (!split_res.has_value()) {
                    return tl::unexpected(split_res.error());
                }
                splits.push_back(SplitEntry{trk.id, std::move(split_res.value().first),
                                            std::move(split_res.value().second), cl.link_id});
            }
        }
    }

    // Coordinate links for right halves of splits
    std::unordered_map<model::LinkId, std::vector<std::size_t>> right_link_groups;
    for (std::size_t i = 0; i < splits.size(); ++i) {
        if (splits[i].orig_link.has_value()) {
            right_link_groups[*splits[i].orig_link].push_back(i);
        }
    }

    for (auto& [orig_lid, indices] : right_link_groups) {
        if (indices.size() >= 2) {
            const auto new_lid = model::generate_id<model::LinkId>(ids);
            for (std::size_t idx : indices) {
                splits[idx].right.link_id = new_lid;
            }
        }
    }

    // Apply splits to scratch tracks
    for (const auto& sp : splits) {
        auto* trk = st.find_track(sp.track);
        auto it = std::find_if(trk->clips.begin(), trk->clips.end(),
                               [&sp](const model::Clip& c) { return c.id == sp.left.id; });
        if (it != trk->clips.end()) {
            *it = sp.left;
            trk->clips.push_back(sp.right);
            trk->touched = true;
        }
    }

    // Shift all clips with start >= snapped_at by delta on scope tracks
    for (auto& trk : st.tracks()) {
        if (!scope_tracks.contains(trk.id)) {
            continue;
        }
        for (auto& cl : trk.clips) {
            if (cl.start >= snapped_at) {
                const auto shifted = detail::add_time(cl.start, delta);
                if (!shifted.has_value()) {
                    return tl::unexpected(shifted.error());
                }
                cl.start = shifted.value();
                trk.touched = true;
            }
        }
    }

    // Add new clips at snapped_at
    const std::optional<model::LinkId> common_link =
        (clips.size() > 1) ? std::optional(model::generate_id<model::LinkId>(ids)) : std::nullopt;

    for (const auto& pc : clips) {
        auto* trk = st.find_track(pc.track);
        const auto snap_dur = st.snap_duration(pc.clip.duration).value();

        model::Clip new_clip = pc.clip;
        new_clip.id = model::generate_id<model::ClipId>(ids);
        new_clip.start = snapped_at;
        new_clip.duration = snap_dur;
        new_clip.link_id = common_link;

        const auto val_status = st.validate_clip_media_and_source(new_clip);
        if (!val_status.has_value()) {
            return tl::unexpected(val_status.error());
        }

        trk->clips.push_back(std::move(new_clip));
        trk->touched = true;
    }

    const auto overlap_status = st.check_overlaps_on_touched_tracks();
    if (!overlap_status.has_value()) {
        return tl::unexpected(overlap_status.error());
    }

    const auto repair_status = st.repair_links();
    if (!repair_status.has_value()) {
        return tl::unexpected(repair_status.error());
    }

    return st.diff_to_changes(label());
}

// ============================================================================
// OverwriteClips
// ============================================================================

core::Result<ChangeSet> OverwriteClips::build(const model::Project& project,
                                              core::UuidGenerator& ids) const {
    if (clips.empty()) {
        return core::make_error(core::ErrorCode::InvalidArgument, "clips list cannot be empty");
    }

    auto st_res = detail::ScratchTimeline::create(project, sequence, ids);
    if (!st_res.has_value()) {
        return tl::unexpected(st_res.error());
    }
    auto& st = st_res.value();

    const auto snapped_at_res = st.snap_time(at);
    if (!snapped_at_res.has_value()) {
        return tl::unexpected(snapped_at_res.error());
    }
    const auto snapped_at = snapped_at_res.value();

    for (const auto& pc : clips) {
        auto* trk = st.find_track(pc.track);
        if (trk == nullptr) {
            return core::make_error(core::ErrorCode::NotFound, "destination track not found");
        }
        if (trk->locked) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "cannot overwrite clips on locked track");
        }
        if (!st.is_clip_compatible(trk->kind, pc.clip.content)) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "clip kind incompatible with track kind");
        }
        const auto snap_dur_res = st.snap_duration(pc.clip.duration);
        if (!snap_dur_res.has_value()) {
            return tl::unexpected(snap_dur_res.error());
        }
    }

    const std::optional<model::LinkId> common_link =
        (clips.size() > 1) ? std::optional(model::generate_id<model::LinkId>(ids)) : std::nullopt;

    for (const auto& pc : clips) {
        auto* trk = st.find_track(pc.track);
        const auto snap_dur = st.snap_duration(pc.clip.duration).value();
        const auto ov_start = snapped_at;
        const auto ov_end = detail::add_time(ov_start, snap_dur).value();

        std::vector<model::Clip> updated_clips;
        for (auto& existing : trk->clips) {
            const auto ex_end = model::clip_end(existing).value();

            // Check if existing clip intersects [ov_start, ov_end)
            if (ex_end <= ov_start || existing.start >= ov_end) {
                // No overlap
                updated_clips.push_back(std::move(existing));
            } else if (ov_start <= existing.start && ex_end <= ov_end) {
                // Fully covered -> remove
                if (existing.link_id.has_value()) {
                    st.touched_links().insert(*existing.link_id);
                }
                trk->touched = true;
            } else if (existing.start < ov_start && ov_end < ex_end) {
                // Strictly contains -> split middle out
                model::Clip left = existing;
                left.duration = detail::diff_time(ov_start, existing.start).value();

                model::Clip right = existing;
                right.id = model::generate_id<model::ClipId>(ids);
                right.start = ov_end;
                right.duration = detail::diff_time(ex_end, ov_end).value();
                right.link_id = std::nullopt;

                const model::ClipKind c_kind = model::kind_of(existing.content);
                if (c_kind == model::ClipKind::Image || c_kind == model::ClipKind::Text) {
                    right.source_in = model::SourceTime::zero();
                } else {
                    const auto c_offset =
                        model::ClipTime::from_ticks(ov_end.ticks() - existing.start.ticks());
                    right.source_in = st.find_clip(existing.id).second != nullptr
                                          ? model::clip_to_source(existing, c_offset).value()
                                          : existing.source_in;
                }

                if (c_kind == model::ClipKind::Audio) {
                    std::get<model::AudioContent>(left.content).fade_out = core::Duration::zero();
                    std::get<model::AudioContent>(right.content).fade_in = core::Duration::zero();
                }

                if (existing.link_id.has_value()) {
                    st.touched_links().insert(*existing.link_id);
                }

                updated_clips.push_back(std::move(left));
                updated_clips.push_back(std::move(right));
                trk->touched = true;
            } else if (ov_start <= existing.start && existing.start < ov_end && ov_end < ex_end) {
                // Head covered -> trim head
                auto trim_st = st.trim_head(existing, ov_end);
                if (!trim_st.has_value()) {
                    return tl::unexpected(trim_st.error());
                }
                updated_clips.push_back(std::move(existing));
                trk->touched = true;
            } else if (existing.start < ov_start && ov_start < ex_end && ex_end <= ov_end) {
                // Tail covered -> trim tail
                auto trim_st = st.trim_tail(existing, ov_start);
                if (!trim_st.has_value()) {
                    return tl::unexpected(trim_st.error());
                }
                updated_clips.push_back(std::move(existing));
                trk->touched = true;
            }
        }

        model::Clip new_clip = pc.clip;
        new_clip.id = model::generate_id<model::ClipId>(ids);
        new_clip.start = ov_start;
        new_clip.duration = snap_dur;
        new_clip.link_id = common_link;

        const auto val_status = st.validate_clip_media_and_source(new_clip);
        if (!val_status.has_value()) {
            return tl::unexpected(val_status.error());
        }

        updated_clips.push_back(std::move(new_clip));
        trk->clips = std::move(updated_clips);
        trk->touched = true;
    }

    const auto overlap_status = st.check_overlaps_on_touched_tracks();
    if (!overlap_status.has_value()) {
        return tl::unexpected(overlap_status.error());
    }

    const auto repair_status = st.repair_links();
    if (!repair_status.has_value()) {
        return tl::unexpected(repair_status.error());
    }

    return st.diff_to_changes(label());
}

// ============================================================================
// MoveClips
// ============================================================================

core::Result<ChangeSet> MoveClips::build(const model::Project& project,
                                         core::UuidGenerator& ids) const {
    if (moves.empty()) {
        return core::make_error(core::ErrorCode::InvalidArgument, "moves list cannot be empty");
    }

    auto st_res = detail::ScratchTimeline::create(project, sequence, ids);
    if (!st_res.has_value()) {
        return tl::unexpected(st_res.error());
    }
    auto& st = st_res.value();

    std::unordered_set<model::ClipId> seen_moves;
    for (const auto& m : moves) {
        if (!seen_moves.insert(m.clip).second) {
            return core::make_error(core::ErrorCode::InvalidArgument, "duplicate clip in moves");
        }
        const auto [src_trk, cl] = st.find_clip(m.clip);
        if (cl == nullptr || src_trk == nullptr) {
            return core::make_error(core::ErrorCode::NotFound, "clip to move not found");
        }
        auto* dest_trk = st.find_track(m.new_track);
        if (dest_trk == nullptr) {
            return core::make_error(core::ErrorCode::NotFound, "destination track not found");
        }
        if (src_trk->locked || dest_trk->locked) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "cannot move clips on locked track");
        }
        const auto snap_start_res = st.snap_time(m.new_start);
        if (!snap_start_res.has_value()) {
            return tl::unexpected(snap_start_res.error());
        }
        if (snap_start_res.value().ticks() < 0) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "clip start cannot be negative");
        }
        if (!st.is_clip_compatible(dest_trk->kind, cl->content)) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "clip kind incompatible with destination track kind");
        }
    }

    struct ResolvedMove {
        model::ClipId clip_id;
        model::TimelineTime new_start;
        model::TrackId new_track;
    };
    std::vector<ResolvedMove> resolved;
    resolved.reserve(moves.size());

    for (const auto& m : moves) {
        const auto snap_start = st.snap_time(m.new_start).value();
        resolved.push_back(ResolvedMove{m.clip, snap_start, m.new_track});
    }

    if (!ignore_links) {
        std::unordered_set<model::ClipId> resolved_ids = seen_moves;
        for (const auto& m : moves) {
            const auto [src_trk, cl] = st.find_clip(m.clip);
            if (cl != nullptr && cl->link_id.has_value()) {
                const auto snapped_new = st.snap_time(m.new_start).value();
                const auto delta = detail::diff_time(snapped_new, cl->start).value();

                for (const auto& trk : st.tracks()) {
                    for (const auto& other_cl : trk.clips) {
                        if (other_cl.link_id == cl->link_id &&
                            !resolved_ids.contains(other_cl.id)) {
                            if (trk.locked) {
                                return core::make_error(core::ErrorCode::InvalidArgument,
                                                        "cannot move linked clip on locked track");
                            }
                            const auto partner_new_start =
                                detail::add_time(other_cl.start, delta);
                            if (!partner_new_start.has_value()) {
                                return tl::unexpected(partner_new_start.error());
                            }
                            if (partner_new_start.value().ticks() < 0) {
                                return core::make_error(core::ErrorCode::InvalidArgument,
                                                        "clip start cannot be negative");
                            }
                            resolved.push_back(
                                ResolvedMove{other_cl.id, partner_new_start.value(), trk.id});
                            resolved_ids.insert(other_cl.id);
                        }
                    }
                }
            }
        }
    }

    // Check if any clip actually changes position or track
    bool any_changed = false;
    for (const auto& rm : resolved) {
        const auto [src_trk, cl] = st.find_clip(rm.clip_id);
        if (src_trk->id != rm.new_track || cl->start != rm.new_start) {
            any_changed = true;
            break;
        }
    }
    if (!any_changed) {
        return ChangeSet{label(), {}};
    }

    // Remove all moving clips from their source tracks
    std::vector<model::Clip> detached_clips;
    detached_clips.reserve(resolved.size());

    for (const auto& rm : resolved) {
        auto [src_trk, cl] = st.find_clip(rm.clip_id);
        detached_clips.push_back(*cl);
        auto it = std::find_if(src_trk->clips.begin(), src_trk->clips.end(),
                               [&rm](const model::Clip& c) { return c.id == rm.clip_id; });
        src_trk->clips.erase(it);
        src_trk->touched = true;
    }

    // Insert moving clips into new tracks at new start positions
    for (std::size_t i = 0; i < resolved.size(); ++i) {
        const auto& rm = resolved[i];
        auto clip_to_insert = detached_clips[i];
        clip_to_insert.start = rm.new_start;

        auto* dest_trk = st.find_track(rm.new_track);
        dest_trk->clips.push_back(std::move(clip_to_insert));
        dest_trk->touched = true;
    }

    const auto overlap_status = st.check_overlaps_on_touched_tracks();
    if (!overlap_status.has_value()) {
        return tl::unexpected(overlap_status.error());
    }

    const auto repair_status = st.repair_links();
    if (!repair_status.has_value()) {
        return tl::unexpected(repair_status.error());
    }

    return st.diff_to_changes(label());
}

// ============================================================================
// DeleteClips
// ============================================================================

core::Result<ChangeSet> DeleteClips::build(const model::Project& project,
                                           core::UuidGenerator& ids) const {
    if (clip_ids.empty()) {
        return core::make_error(core::ErrorCode::InvalidArgument, "clip_ids list cannot be empty");
    }

    auto st_res = detail::ScratchTimeline::create(project, sequence, ids);
    if (!st_res.has_value()) {
        return tl::unexpected(st_res.error());
    }
    auto& st = st_res.value();

    std::vector<model::ClipId> unique_ids;
    for (const auto& cid : clip_ids) {
        if (std::find(unique_ids.begin(), unique_ids.end(), cid) == unique_ids.end()) {
            unique_ids.push_back(cid);
        }
    }

    for (const auto& cid : unique_ids) {
        const auto [trk, cl] = st.find_clip(cid);
        if (cl == nullptr) {
            return core::make_error(core::ErrorCode::NotFound, "clip to delete not found");
        }
    }

    std::vector<model::ClipId> to_delete =
        ignore_links ? unique_ids : st.expand_links(unique_ids);

    for (const auto& cid : to_delete) {
        const auto [trk, cl] = st.find_clip(cid);
        if (trk->locked) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "cannot delete clip on locked track");
        }
    }

    for (const auto& cid : to_delete) {
        auto [trk, cl] = st.find_clip(cid);
        if (cl->link_id.has_value()) {
            st.touched_links().insert(*cl->link_id);
        }
        auto it = std::find_if(trk->clips.begin(), trk->clips.end(),
                               [&cid](const model::Clip& c) { return c.id == cid; });
        if (it != trk->clips.end()) {
            trk->clips.erase(it);
            trk->touched = true;
        }
    }

    const auto repair_status = st.repair_links();
    if (!repair_status.has_value()) {
        return tl::unexpected(repair_status.error());
    }

    return st.diff_to_changes(label());
}

// ============================================================================
// SplitClip
// ============================================================================

core::Result<ChangeSet> SplitClip::build(const model::Project& project,
                                         core::UuidGenerator& ids) const {
    auto st_res = detail::ScratchTimeline::create(project, sequence, ids);
    if (!st_res.has_value()) {
        return tl::unexpected(st_res.error());
    }
    auto& st = st_res.value();

    const auto [orig_trk, orig_clip] = st.find_clip(clip);
    if (orig_clip == nullptr) {
        return core::make_error(core::ErrorCode::NotFound, "clip to split not found");
    }

    const auto snapped_at_res = st.snap_time(at);
    if (!snapped_at_res.has_value()) {
        return tl::unexpected(snapped_at_res.error());
    }
    const auto snapped_at = snapped_at_res.value();

    const auto end_res = model::clip_end(*orig_clip);
    if (!end_res.has_value()) {
        return tl::unexpected(end_res.error());
    }

    if (snapped_at <= orig_clip->start || snapped_at >= *end_res) {
        return core::make_error(core::ErrorCode::InvalidArgument,
                                "split time must be strictly inside clip");
    }

    if (orig_trk->locked) {
        return core::make_error(core::ErrorCode::InvalidArgument,
                                "cannot split clip on locked track");
    }

    std::vector<model::ClipId> targets = {clip};
    if (!ignore_links && orig_clip->link_id.has_value()) {
        for (const auto& trk : st.tracks()) {
            for (const auto& other_cl : trk.clips) {
                if (other_cl.link_id == orig_clip->link_id && other_cl.id != clip) {
                    const auto other_end = model::clip_end(other_cl).value();
                    if (other_cl.start < snapped_at && snapped_at < other_end) {
                        if (trk.locked) {
                            return core::make_error(core::ErrorCode::InvalidArgument,
                                                    "cannot split linked clip on locked track");
                        }
                        targets.push_back(other_cl.id);
                    }
                }
            }
        }
    }

    struct SplitResultItem {
        model::TrackId track;
        model::Clip left;
        model::Clip right;
        std::optional<model::LinkId> orig_link;
    };
    std::vector<SplitResultItem> results;
    results.reserve(targets.size());

    for (const auto& tid : targets) {
        const auto [trk, cl] = st.find_clip(tid);
        auto split_res = st.split_clip(*cl, snapped_at);
        if (!split_res.has_value()) {
            return tl::unexpected(split_res.error());
        }
        results.push_back(SplitResultItem{trk->id, std::move(split_res.value().first),
                                          std::move(split_res.value().second), cl->link_id});
    }

    // Link handling for right halves
    std::unordered_map<model::LinkId, std::vector<std::size_t>> right_groups;
    for (std::size_t i = 0; i < results.size(); ++i) {
        if (results[i].orig_link.has_value()) {
            right_groups[*results[i].orig_link].push_back(i);
        }
    }

    for (auto& [orig_lid, idx_vec] : right_groups) {
        if (idx_vec.size() >= 2) {
            const auto new_lid = model::generate_id<model::LinkId>(ids);
            for (std::size_t idx : idx_vec) {
                results[idx].right.link_id = new_lid;
            }
        }
    }

    // Apply to scratch tracks
    for (const auto& item : results) {
        auto* trk = st.find_track(item.track);
        auto it = std::find_if(trk->clips.begin(), trk->clips.end(),
                               [&item](const model::Clip& c) { return c.id == item.left.id; });
        if (it != trk->clips.end()) {
            *it = item.left;
            trk->clips.push_back(item.right);
            trk->touched = true;
        }
    }

    const auto overlap_status = st.check_overlaps_on_touched_tracks();
    if (!overlap_status.has_value()) {
        return tl::unexpected(overlap_status.error());
    }

    const auto repair_status = st.repair_links();
    if (!repair_status.has_value()) {
        return tl::unexpected(repair_status.error());
    }

    return st.diff_to_changes(label());
}

}  // namespace nxtcut::commands
