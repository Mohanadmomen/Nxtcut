#include <nxtcut/commands/timeline_commands.hpp>
#include <nxtcut/core/error.hpp>
#include <nxtcut/core/frame_time.hpp>
#include <nxtcut/model/checked_arithmetic.hpp>
#include <nxtcut/model/clip.hpp>
#include <nxtcut/model/ids.hpp>

#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

#include "timeline_support.hpp"

namespace nxtcut::commands {

// ============================================================================
// RateStretch
// ============================================================================

core::Result<ChangeSet> RateStretch::build(const model::Project& project,
                                           core::UuidGenerator& ids) const {
    auto st_res = detail::ScratchTimeline::create(project, sequence, ids);
    if (!st_res.has_value()) {
        return tl::unexpected(st_res.error());
    }
    auto& st = st_res.value();

    const auto [orig_trk, orig_clip] = st.find_clip(clip);
    if (orig_clip == nullptr || orig_trk == nullptr) {
        return core::make_error(core::ErrorCode::NotFound, "clip to rate stretch not found");
    }

    if (orig_trk->locked) {
        return core::make_error(core::ErrorCode::InvalidArgument,
                                "cannot rate stretch clip on locked track");
    }

    const auto snapped_edge_res = st.snap_time(new_edge);
    if (!snapped_edge_res.has_value()) {
        return tl::unexpected(snapped_edge_res.error());
    }
    const auto snapped_edge = snapped_edge_res.value();

    const auto orig_end_res = model::clip_end(*orig_clip);
    if (!orig_end_res.has_value()) {
        return tl::unexpected(orig_end_res.error());
    }
    const auto orig_end = orig_end_res.value();
    const auto orig_start = orig_clip->start;

    const auto frame_dur_res = core::frame_duration(st.sequence().frame_rate);
    if (!frame_dur_res.has_value()) {
        return tl::unexpected(frame_dur_res.error());
    }
    const auto frame_dur = frame_dur_res.value();

    // Compute delta and dur_change
    core::Duration delta = core::Duration::zero();
    core::Duration dur_change = core::Duration::zero();

    if (edge == TrimEdge::Tail) {
        if (snapped_edge <= orig_start) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "tail stretch must be after clip start");
        }
        const auto new_dur_res = detail::diff_time(snapped_edge, orig_start);
        if (!new_dur_res.has_value()) {
            return tl::unexpected(new_dur_res.error());
        }
        if (*new_dur_res < frame_dur) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "stretched duration less than one frame");
        }
        const auto delta_res = detail::diff_time(snapped_edge, orig_end);
        if (!delta_res.has_value()) {
            return tl::unexpected(delta_res.error());
        }
        delta = *delta_res;
        dur_change = delta;
    } else {
        if (snapped_edge >= orig_end) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "head stretch must be before clip end");
        }
        const auto new_dur_res = detail::diff_time(orig_end, snapped_edge);
        if (!new_dur_res.has_value()) {
            return tl::unexpected(new_dur_res.error());
        }
        if (*new_dur_res < frame_dur) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "stretched duration less than one frame");
        }
        const auto delta_res = detail::diff_time(snapped_edge, orig_start);
        if (!delta_res.has_value()) {
            return tl::unexpected(delta_res.error());
        }
        delta = *delta_res;
        const auto dur_change_ticks = model::detail::checked_sub(0, delta.ticks());
        if (!dur_change_ticks.has_value()) {
            return core::make_error(core::ErrorCode::Overflow, "time arithmetic overflow");
        }
        dur_change = core::Duration::from_ticks(*dur_change_ticks);
    }

    // Check if stretch results in no change
    if (dur_change.ticks() == 0 && delta.ticks() == 0) {
        return ChangeSet{label(), {}};
    }

    // Collect clips to stretch (clip plus linked partners unless ignore_links)
    std::vector<model::ClipId> targets = {clip};
    if (!ignore_links && orig_clip->link_id.has_value()) {
        for (const auto& trk : st.tracks()) {
            for (const auto& other_cl : trk.clips) {
                if (other_cl.link_id == orig_clip->link_id && other_cl.id != clip) {
                    if (trk.locked) {
                        return core::make_error(core::ErrorCode::InvalidArgument,
                                                "cannot rate stretch linked clip on locked track");
                    }
                    targets.push_back(other_cl.id);
                }
            }
        }
    }

    // Apply stretch to collected clips
    for (const auto& tid : targets) {
        auto [trk, cl] = st.find_clip(tid);

        if (edge == TrimEdge::Tail) {
            const auto cl_end = detail::end_of(*cl);
            const auto p_new_end = detail::add_time(cl_end, delta);
            if (!p_new_end.has_value()) {
                return tl::unexpected(p_new_end.error());
            }
            if (*p_new_end <= cl->start) {
                return core::make_error(core::ErrorCode::InvalidArgument,
                                        "tail stretch must be after clip start");
            }
            const auto p_new_dur = detail::diff_time(*p_new_end, cl->start);
            if (!p_new_dur.has_value()) {
                return tl::unexpected(p_new_dur.error());
            }
            if (*p_new_dur < frame_dur) {
                return core::make_error(core::ErrorCode::InvalidArgument,
                                        "stretched duration less than one frame");
            }
            const auto stretch_status = detail::stretch_to_duration(*cl, *p_new_dur);
            if (!stretch_status.has_value()) {
                return tl::unexpected(stretch_status.error());
            }
        } else {
            if (!ripple) {
                const auto p_new_start = detail::add_time(cl->start, delta);
                if (!p_new_start.has_value()) {
                    return tl::unexpected(p_new_start.error());
                }
                const auto cl_end = detail::end_of(*cl);
                if (*p_new_start >= cl_end) {
                    return core::make_error(core::ErrorCode::InvalidArgument,
                                            "head stretch must be before clip end");
                }
                const auto p_new_dur = detail::diff_time(cl_end, *p_new_start);
                if (!p_new_dur.has_value()) {
                    return tl::unexpected(p_new_dur.error());
                }
                if (*p_new_dur < frame_dur) {
                    return core::make_error(core::ErrorCode::InvalidArgument,
                                            "stretched duration less than one frame");
                }
                cl->start = *p_new_start;
                const auto stretch_status = detail::stretch_to_duration(*cl, *p_new_dur);
                if (!stretch_status.has_value()) {
                    return tl::unexpected(stretch_status.error());
                }
            } else {
                // Ripple head stretch: clip keeps start, duration changes by dur_change
                const auto new_p_dur_ticks =
                    model::detail::checked_add(cl->duration.ticks(), dur_change.ticks());
                if (!new_p_dur_ticks.has_value()) {
                    return core::make_error(core::ErrorCode::Overflow, "time arithmetic overflow");
                }
                if (*new_p_dur_ticks <= 0) {
                    return core::make_error(core::ErrorCode::InvalidArgument,
                                            "head stretch must be before clip end");
                }
                const auto new_p_dur = core::Duration::from_ticks(*new_p_dur_ticks);
                if (new_p_dur < frame_dur) {
                    return core::make_error(core::ErrorCode::InvalidArgument,
                                            "stretched duration less than one frame");
                }
                const auto stretch_status = detail::stretch_to_duration(*cl, new_p_dur);
                if (!stretch_status.has_value()) {
                    return tl::unexpected(stretch_status.error());
                }
            }
        }

        const auto val_status = st.validate_clip_media_and_source(*cl);
        if (!val_status.has_value()) {
            return tl::unexpected(val_status.error());
        }
        trk->touched = true;
    }

    // Ripple shift later material
    if (ripple) {
        std::unordered_set<model::TrackId> scope_tracks;
        if (scope == RippleScope::AllUnlockedTracks) {
            for (const auto& trk : st.tracks()) {
                if (!trk.locked) {
                    scope_tracks.insert(trk.id);
                }
            }
        } else {
            for (const auto& tid : targets) {
                const auto [trk, _] = st.find_clip(tid);
                scope_tracks.insert(trk->id);
            }
        }

        std::unordered_set<model::ClipId> target_ids(targets.begin(), targets.end());

        for (auto& trk : st.tracks()) {
            if (!scope_tracks.contains(trk.id)) {
                continue;
            }

            for (auto& other_cl : trk.clips) {
                if (!target_ids.contains(other_cl.id) && other_cl.start >= orig_end) {
                    const auto shifted = detail::add_time(other_cl.start, dur_change);
                    if (!shifted.has_value()) {
                        return tl::unexpected(shifted.error());
                    }
                    other_cl.start = shifted.value();
                    trk.touched = true;
                }
            }
        }
    }

    const auto overlap_status = st.check_overlaps_on_touched_tracks();
    if (!overlap_status.has_value()) {
        return tl::unexpected(overlap_status.error());
    }

    return st.diff_to_changes(label());
}

// ============================================================================
// ShiftTrackClips
// ============================================================================

core::Result<ChangeSet> ShiftTrackClips::build(const model::Project& project,
                                               core::UuidGenerator& ids) const {
    auto st_res = detail::ScratchTimeline::create(project, sequence, ids);
    if (!st_res.has_value()) {
        return tl::unexpected(st_res.error());
    }
    auto& st = st_res.value();

    auto* target_trk = st.find_track(track);
    if (target_trk == nullptr) {
        return core::make_error(core::ErrorCode::NotFound, "track to shift not found");
    }

    if (target_trk->locked) {
        return core::make_error(core::ErrorCode::InvalidArgument,
                                "cannot shift clips on locked track");
    }

    const auto snapped_at_res = st.snap_time(at);
    if (!snapped_at_res.has_value()) {
        return tl::unexpected(snapped_at_res.error());
    }
    const auto snapped_at = snapped_at_res.value();
    if (snapped_at.ticks() < 0) {
        return core::make_error(core::ErrorCode::InvalidArgument,
                                "shift at time cannot be negative");
    }

    const auto snapped_delta_res = st.snap_delta(delta);
    if (!snapped_delta_res.has_value()) {
        return tl::unexpected(snapped_delta_res.error());
    }
    const auto snapped_delta = snapped_delta_res.value();

    // Shifted set = every clip on track with start >= snap(at).
    std::vector<model::ClipId> seed_clips;
    for (const auto& cl : target_trk->clips) {
        if (cl.start >= snapped_at) {
            seed_clips.push_back(cl.id);
        }
    }

    if (seed_clips.empty() || snapped_delta.ticks() == 0) {
        return ChangeSet{label(), {}};
    }

    // Collect all clips to shift: seed clips + linked partners unless ignore_links.
    std::unordered_set<model::ClipId> shift_set;
    std::vector<model::ClipId> to_shift;

    for (const auto cid : seed_clips) {
        if (shift_set.insert(cid).second) {
            to_shift.push_back(cid);
        }
    }

    if (!ignore_links) {
        std::unordered_set<model::LinkId> link_ids;
        for (const auto cid : seed_clips) {
            const auto [_, cl] = st.find_clip(cid);
            if (cl != nullptr && cl->link_id.has_value()) {
                link_ids.insert(*cl->link_id);
            }
        }

        if (!link_ids.empty()) {
            for (const auto& trk : st.tracks()) {
                for (const auto& cl : trk.clips) {
                    if (cl.link_id.has_value() && link_ids.contains(*cl.link_id)) {
                        if (trk.locked) {
                            return core::make_error(core::ErrorCode::InvalidArgument,
                                                    "cannot shift linked clip on locked track");
                        }
                        if (shift_set.insert(cl.id).second) {
                            to_shift.push_back(cl.id);
                        }
                    }
                }
            }
        }
    }

    // Apply delta to each shifted clip
    for (const auto cid : to_shift) {
        auto [trk, cl] = st.find_clip(cid);
        const auto new_start_res = detail::add_time(cl->start, snapped_delta);
        if (!new_start_res.has_value()) {
            return tl::unexpected(new_start_res.error());
        }
        cl->start = new_start_res.value();
        trk->touched = true;
    }

    const auto overlap_status = st.check_overlaps_on_touched_tracks();
    if (!overlap_status.has_value()) {
        return tl::unexpected(overlap_status.error());
    }

    return st.diff_to_changes(label());
}

}  // namespace nxtcut::commands
