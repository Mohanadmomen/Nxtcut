#include <nxtcut/commands/timeline_commands.hpp>
#include <nxtcut/core/error.hpp>
#include <nxtcut/core/frame_time.hpp>
#include <nxtcut/core/mul_div.hpp>
#include <nxtcut/model/checked_arithmetic.hpp>
#include <nxtcut/model/clip.hpp>
#include <nxtcut/model/equality.hpp>
#include <nxtcut/model/ids.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include "timeline_support.hpp"

namespace nxtcut::commands {

// ============================================================================
// TrimClip
// ============================================================================

core::Result<ChangeSet> TrimClip::build(const model::Project& project,
                                        core::UuidGenerator& ids) const {
    auto st_res = detail::ScratchTimeline::create(project, sequence, ids);
    if (!st_res.has_value()) {
        return tl::unexpected(st_res.error());
    }
    auto& st = st_res.value();

    const auto [orig_trk, orig_clip] = st.find_clip(clip);
    if (orig_clip == nullptr || orig_trk == nullptr) {
        return core::make_error(core::ErrorCode::NotFound, "clip to trim not found");
    }

    if (orig_trk->locked) {
        return core::make_error(core::ErrorCode::InvalidArgument,
                                "cannot trim clip on locked track");
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

    // Compute delta and duration change
    core::Duration delta = core::Duration::zero();
    core::Duration dur_change = core::Duration::zero();

    if (edge == TrimEdge::Tail) {
        if (snapped_edge <= orig_start) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "tail trim must be after clip start");
        }
        const auto new_dur_res = detail::diff_time(snapped_edge, orig_start);
        if (!new_dur_res.has_value()) {
            return tl::unexpected(new_dur_res.error());
        }
        if (*new_dur_res < frame_dur) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "trimmed duration less than one frame");
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
                                    "head trim must be before clip end");
        }
        const auto new_dur_res = detail::diff_time(orig_end, snapped_edge);
        if (!new_dur_res.has_value()) {
            return tl::unexpected(new_dur_res.error());
        }
        if (*new_dur_res < frame_dur) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "trimmed duration less than one frame");
        }
        const auto delta_res = detail::diff_time(snapped_edge, orig_start);
        if (!delta_res.has_value()) {
            return tl::unexpected(delta_res.error());
        }
        delta = *delta_res;
        // dur_change = new_dur - orig_dur = -delta
        const auto dur_change_ticks = model::detail::checked_sub(0, delta.ticks());
        if (!dur_change_ticks.has_value()) {
            return core::make_error(core::ErrorCode::Overflow, "time arithmetic overflow");
        }
        dur_change = core::Duration::from_ticks(*dur_change_ticks);
    }

    // Check if trim results in no change
    if (dur_change.ticks() == 0 && delta.ticks() == 0) {
        return ChangeSet{label(), {}};
    }

    // Collect clips to trim (clip plus linked partners unless ignore_links)
    std::vector<model::ClipId> targets = {clip};
    if (!ignore_links && orig_clip->link_id.has_value()) {
        for (const auto& trk : st.tracks()) {
            for (const auto& other_cl : trk.clips) {
                if (other_cl.link_id == orig_clip->link_id && other_cl.id != clip) {
                    if (trk.locked) {
                        return core::make_error(core::ErrorCode::InvalidArgument,
                                                "cannot trim linked clip on locked track");
                    }
                    targets.push_back(other_cl.id);
                }
            }
        }
    }

    // Apply trim to collected clips
    for (const auto& tid : targets) {
        auto [trk, cl] = st.find_clip(tid);
        const auto cl_end = detail::end_of(*cl);

        if (edge == TrimEdge::Tail) {
            const auto p_new_end = detail::add_time(cl_end, delta);
            if (!p_new_end.has_value()) {
                return tl::unexpected(p_new_end.error());
            }
            auto trim_st = st.trim_tail(*cl, p_new_end.value());
            if (!trim_st.has_value()) {
                return tl::unexpected(trim_st.error());
            }
        } else {
            if (!ripple) {
                const auto p_new_start = detail::add_time(cl->start, delta);
                if (!p_new_start.has_value()) {
                    return tl::unexpected(p_new_start.error());
                }
                auto trim_st = st.trim_head(*cl, p_new_start.value());
                if (!trim_st.has_value()) {
                    return tl::unexpected(trim_st.error());
                }
            } else {
                // Ripple head trim: clip keeps start, duration changes by dur_change,
                // source_in advances by delta
                const auto new_p_dur =
                    detail::add_time(model::TimelineTime::zero(), cl->duration + dur_change);
                if (!new_p_dur.has_value()) {
                    return tl::unexpected(new_p_dur.error());
                }
                cl->duration = core::Duration::from_ticks(new_p_dur.value().ticks());

                const model::ClipKind c_kind = model::kind_of(cl->content);
                if (c_kind == model::ClipKind::Image || c_kind == model::ClipKind::Text) {
                    cl->source_in = model::SourceTime::zero();
                } else {
                    const auto scaled_res =
                        core::mul_div(delta.ticks(), cl->speed.numerator(), cl->speed.denominator(),
                                      core::RoundingMode::Floor);
                    if (!scaled_res.has_value()) {
                        return tl::unexpected(scaled_res.error());
                    }
                    const auto new_s_ticks = cl->source_in.ticks() + *scaled_res;
                    if (new_s_ticks < 0) {
                        return core::make_error(core::ErrorCode::InvalidArgument,
                                                "source range extends before zero");
                    }
                    cl->source_in = model::SourceTime::from_ticks(new_s_ticks);
                }

                detail::fit_audio_fades(*cl);
                if (cl->link_id.has_value()) {
                    st.touched_links().insert(*cl->link_id);
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

    const auto repair_status = st.repair_links();
    if (!repair_status.has_value()) {
        return tl::unexpected(repair_status.error());
    }

    return st.diff_to_changes(label());
}

// ============================================================================
// RippleDeleteClips
// ============================================================================

core::Result<ChangeSet> RippleDeleteClips::build(const model::Project& project,
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

    std::vector<model::ClipId> to_delete = ignore_links ? unique_ids : st.expand_links(unique_ids);

    for (const auto& cid : to_delete) {
        const auto [trk, cl] = st.find_clip(cid);
        if (trk->locked) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "cannot delete clip on locked track");
        }
    }

    // Collect deleted intervals across all tracks
    struct Interval {
        std::int64_t start;
        std::int64_t end;
    };
    std::vector<Interval> raw_intervals;
    std::unordered_set<model::TrackId> edited_track_ids;

    for (const auto& cid : to_delete) {
        const auto [trk, cl] = st.find_clip(cid);
        const auto cl_end = detail::end_of(*cl);
        raw_intervals.push_back(Interval{cl->start.ticks(), cl_end.ticks()});
        edited_track_ids.insert(trk->id);
    }

    std::sort(raw_intervals.begin(), raw_intervals.end(),
              [](const Interval& a, const Interval& b) { return a.start < b.start; });

    // Merge overlapping / adjacent intervals into disjoint sorted intervals
    std::vector<Interval> merged;
    for (const auto& iv : raw_intervals) {
        if (merged.empty() || merged.back().end < iv.start) {
            merged.push_back(iv);
        } else if (iv.end > merged.back().end) {
            merged.back().end = iv.end;
        }
    }

    // Remove deleted clips from scratch tracks
    std::unordered_set<model::ClipId> del_set(to_delete.begin(), to_delete.end());
    for (auto& trk : st.tracks()) {
        std::vector<model::Clip> remaining;
        for (auto& cl : trk.clips) {
            if (del_set.contains(cl.id)) {
                if (cl.link_id.has_value()) {
                    st.touched_links().insert(*cl.link_id);
                }
                trk.touched = true;
            } else {
                remaining.push_back(std::move(cl));
            }
        }
        trk.clips = std::move(remaining);
    }

    // Determine scope tracks (unlocked tracks)
    std::unordered_set<model::TrackId> scope_tracks;
    if (scope == RippleScope::AllUnlockedTracks) {
        for (const auto& trk : st.tracks()) {
            if (!trk.locked) {
                scope_tracks.insert(trk.id);
            }
        }
    } else {
        for (const auto& tid : edited_track_ids) {
            const auto* trk = st.find_track(tid);
            if (trk != nullptr && !trk->locked) {
                scope_tracks.insert(tid);
            }
        }
    }

    // Shift remaining clips on scope tracks
    for (auto& trk : st.tracks()) {
        if (!scope_tracks.contains(trk.id)) {
            continue;
        }

        for (auto& cl : trk.clips) {
            const auto cl_start = cl.start.ticks();
            const auto cl_end = detail::end_of(cl).ticks();

            // Check if clip overlaps any deleted interval
            bool overlaps_any = false;
            for (const auto& m_iv : merged) {
                if (std::max(cl_start, m_iv.start) < std::min(cl_end, m_iv.end)) {
                    overlaps_any = true;
                    break;
                }
            }

            if (overlaps_any) {
                continue;  // Clips overlapping an interval do not shift
            }

            std::int64_t total_shift = 0;
            for (const auto& m_iv : merged) {
                if (m_iv.end <= cl_start) {
                    total_shift += (m_iv.end - m_iv.start);
                }
            }

            if (total_shift > 0) {
                const auto shifted =
                    detail::sub_time(cl.start, core::Duration::from_ticks(total_shift));
                if (!shifted.has_value()) {
                    return tl::unexpected(shifted.error());
                }
                cl.start = shifted.value();
                trk.touched = true;
            }
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

// ============================================================================
// CloseGap
// ============================================================================

core::Result<ChangeSet> CloseGap::build(const model::Project& project,
                                        core::UuidGenerator& ids) const {
    auto st_res = detail::ScratchTimeline::create(project, sequence, ids);
    if (!st_res.has_value()) {
        return tl::unexpected(st_res.error());
    }
    auto& st = st_res.value();

    auto* trk = st.find_track(track);
    if (trk == nullptr) {
        return core::make_error(core::ErrorCode::NotFound, "track not found");
    }
    if (trk->locked) {
        return core::make_error(core::ErrorCode::InvalidArgument,
                                "cannot close gap on locked track");
    }

    const auto snapped_at_res = st.snap_time(at);
    if (!snapped_at_res.has_value()) {
        return tl::unexpected(snapped_at_res.error());
    }
    const auto snapped_at = snapped_at_res.value();

    // Check if at is inside any clip on track
    for (const auto& cl : trk->clips) {
        const auto cl_end = detail::end_of(cl);
        if (cl.start <= snapped_at && snapped_at < cl_end) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "at position is inside a clip");
        }
    }

    // Find next clip and previous clip
    const model::Clip* next_clip = nullptr;
    const model::Clip* prev_clip = nullptr;

    for (const auto& cl : trk->clips) {
        const auto cl_end = detail::end_of(cl);
        if (cl_end <= snapped_at) {
            prev_clip = &cl;
        } else if (cl.start > snapped_at) {
            if (next_clip == nullptr || cl.start < next_clip->start) {
                next_clip = &cl;
            }
        }
    }

    if (next_clip == nullptr) {
        return core::make_error(core::ErrorCode::InvalidArgument, "no clip found after gap");
    }

    const auto gap_start =
        (prev_clip != nullptr) ? detail::end_of(*prev_clip) : model::TimelineTime::zero();
    const auto gap_end = next_clip->start;

    if (snapped_at < gap_start) {
        return core::make_error(core::ErrorCode::InvalidArgument,
                                "at position is not inside the gap");
    }

    const auto gap_length_res = detail::diff_time(gap_end, gap_start);
    if (!gap_length_res.has_value()) {
        return tl::unexpected(gap_length_res.error());
    }
    const auto gap_length = *gap_length_res;
    if (gap_length.ticks() <= 0) {
        return core::make_error(core::ErrorCode::InvalidArgument, "gap length is zero");
    }

    // Scope tracks
    std::unordered_set<model::TrackId> scope_tracks;
    scope_tracks.insert(track);
    if (scope == RippleScope::AllUnlockedTracks) {
        for (const auto& t : st.tracks()) {
            if (!t.locked) {
                scope_tracks.insert(t.id);
            }
        }
    }

    // Shift clips on scope tracks with start >= gap_end earlier by gap_length
    for (auto& t : st.tracks()) {
        if (!scope_tracks.contains(t.id)) {
            continue;
        }
        for (auto& cl : t.clips) {
            if (cl.start >= gap_end) {
                const auto shifted = detail::sub_time(cl.start, gap_length);
                if (!shifted.has_value()) {
                    return tl::unexpected(shifted.error());
                }
                cl.start = shifted.value();
                t.touched = true;
            }
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

// ============================================================================
// JoinClips
// ============================================================================

core::Result<ChangeSet> JoinClips::build(const model::Project& project,
                                         core::UuidGenerator& ids) const {
    if (pairs.empty()) {
        return core::make_error(core::ErrorCode::InvalidArgument, "pairs list cannot be empty");
    }

    auto st_res = detail::ScratchTimeline::create(project, sequence, ids);
    if (!st_res.has_value()) {
        return tl::unexpected(st_res.error());
    }
    auto& st = st_res.value();

    std::unordered_set<model::ClipId> seen_ids;
    for (const auto& p : pairs) {
        if (!seen_ids.insert(p.first).second || !seen_ids.insert(p.second).second) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "clip appears in multiple join pairs");
        }
    }

    for (const auto& p : pairs) {
        const auto [trk1, cl1] = st.find_clip(p.first);
        const auto [trk2, cl2] = st.find_clip(p.second);

        if (cl1 == nullptr || cl2 == nullptr || trk1 == nullptr || trk2 == nullptr) {
            return core::make_error(core::ErrorCode::NotFound, "clip in join pair not found");
        }
        if (trk1->id != trk2->id) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "join pair clips must be on the same track");
        }
        if (trk1->locked) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "cannot join clips on locked track");
        }

        const auto cl1_end = detail::end_of(*cl1);
        if (cl1_end != cl2->start) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "join pair clips must be adjacent");
        }

        const auto k1 = model::kind_of(cl1->content);
        const auto k2 = model::kind_of(cl2->content);
        if (k1 != k2) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "join pair clips must have same content kind");
        }
        if (k1 != model::ClipKind::Video && k1 != model::ClipKind::Audio &&
            k1 != model::ClipKind::Image) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "only video, audio, and image clips can be joined");
        }

        if (cl1->speed != cl2->speed) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "join pair clips must have same speed");
        }

        // Check media and source contiguity
        if (k1 == model::ClipKind::Video) {
            const auto& v1 = std::get<model::VideoContent>(cl1->content);
            const auto& v2 = std::get<model::VideoContent>(cl2->content);
            if (v1.media != v2.media) {
                return core::make_error(core::ErrorCode::InvalidArgument,
                                        "join pair clips must reference same media");
            }
            const auto exp_source =
                model::clip_to_source(*cl1, model::ClipTime::from_ticks(cl1->duration.ticks()));
            if (!exp_source.has_value() || cl2->source_in != exp_source.value()) {
                return core::make_error(core::ErrorCode::InvalidArgument,
                                        "join pair clips must be source contiguous");
            }
        } else if (k1 == model::ClipKind::Audio) {
            const auto& a1 = std::get<model::AudioContent>(cl1->content);
            const auto& a2 = std::get<model::AudioContent>(cl2->content);
            if (a1.media != a2.media) {
                return core::make_error(core::ErrorCode::InvalidArgument,
                                        "join pair clips must reference same media");
            }
            const auto exp_source =
                model::clip_to_source(*cl1, model::ClipTime::from_ticks(cl1->duration.ticks()));
            if (!exp_source.has_value() || cl2->source_in != exp_source.value()) {
                return core::make_error(core::ErrorCode::InvalidArgument,
                                        "join pair clips must be source contiguous");
            }
        } else if (k1 == model::ClipKind::Image) {
            const auto& img1 = std::get<model::ImageContent>(cl1->content);
            const auto& img2 = std::get<model::ImageContent>(cl2->content);
            if (img1.media != img2.media) {
                return core::make_error(core::ErrorCode::InvalidArgument,
                                        "join pair clips must reference same media");
            }
        }

        // Compare everything else identical
        model::Clip copy1 = *cl1;
        model::Clip copy2 = *cl2;
        copy2.id = copy1.id;
        copy2.start = copy1.start;
        copy2.duration = copy1.duration;
        copy2.source_in = copy1.source_in;
        copy2.link_id = copy1.link_id;

        if (k1 == model::ClipKind::Audio) {
            auto& ac1 = std::get<model::AudioContent>(copy1.content);
            auto& ac2 = std::get<model::AudioContent>(copy2.content);
            ac1.fade_in = core::Duration::zero();
            ac1.fade_out = core::Duration::zero();
            ac2.fade_in = core::Duration::zero();
            ac2.fade_out = core::Duration::zero();
        }

        if (!model::identical(copy1, copy2)) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "join pair clips are not otherwise identical");
        }

        // Perform join
        model::Clip joined = *cl1;
        const auto sum_dur =
            model::detail::checked_add(cl1->duration.ticks(), cl2->duration.ticks());
        if (!sum_dur.has_value()) {
            return core::make_error(core::ErrorCode::Overflow,
                                    "joined duration arithmetic overflow");
        }
        joined.duration = core::Duration::from_ticks(*sum_dur);

        if (k1 == model::ClipKind::Audio) {
            auto& a_joined = std::get<model::AudioContent>(joined.content);
            a_joined.fade_in = std::get<model::AudioContent>(cl1->content).fade_in;
            a_joined.fade_out = std::get<model::AudioContent>(cl2->content).fade_out;
        }

        if (cl2->link_id.has_value()) {
            st.touched_links().insert(*cl2->link_id);
        }

        // Replace cl1 with joined and erase cl2
        const model::ClipId id1 = cl1->id;
        const model::ClipId id2 = cl2->id;
        auto it1 = std::find_if(trk1->clips.begin(), trk1->clips.end(),
                                [id1](const model::Clip& c) { return c.id == id1; });
        *it1 = std::move(joined);

        auto it2 = std::find_if(trk1->clips.begin(), trk1->clips.end(),
                                [id2](const model::Clip& c) { return c.id == id2; });
        trk1->clips.erase(it2);
        trk1->touched = true;
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
// LinkClips
// ============================================================================

core::Result<ChangeSet> LinkClips::build(const model::Project& project,
                                         core::UuidGenerator& ids) const {
    std::vector<model::ClipId> unique_ids;
    for (const auto& cid : clip_ids) {
        if (std::find(unique_ids.begin(), unique_ids.end(), cid) == unique_ids.end()) {
            unique_ids.push_back(cid);
        }
    }

    if (unique_ids.size() < 2) {
        return core::make_error(core::ErrorCode::InvalidArgument,
                                "at least 2 distinct clips required to link");
    }

    auto st_res = detail::ScratchTimeline::create(project, sequence, ids);
    if (!st_res.has_value()) {
        return tl::unexpected(st_res.error());
    }
    auto& st = st_res.value();

    for (const auto& cid : unique_ids) {
        const auto [trk, cl] = st.find_clip(cid);
        if (cl == nullptr || trk == nullptr) {
            return core::make_error(core::ErrorCode::NotFound, "clip to link not found");
        }
        if (cl->link_id.has_value()) {
            return core::make_error(core::ErrorCode::InvalidArgument, "clip is already linked");
        }
        if (trk->locked) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "cannot link clip on locked track");
        }
    }

    const auto new_link_id = model::generate_id<model::LinkId>(ids);

    for (const auto& cid : unique_ids) {
        auto [trk, cl] = st.find_clip(cid);
        cl->link_id = new_link_id;
        trk->touched = true;
    }

    return st.diff_to_changes(label());
}

// ============================================================================
// UnlinkClips
// ============================================================================

core::Result<ChangeSet> UnlinkClips::build(const model::Project& project,
                                           core::UuidGenerator& ids) const {
    if (clip_ids.empty()) {
        return core::make_error(core::ErrorCode::InvalidArgument, "clip_ids list cannot be empty");
    }

    auto st_res = detail::ScratchTimeline::create(project, sequence, ids);
    if (!st_res.has_value()) {
        return tl::unexpected(st_res.error());
    }
    auto& st = st_res.value();

    for (const auto& cid : clip_ids) {
        const auto [trk, cl] = st.find_clip(cid);
        if (cl == nullptr) {
            return core::make_error(core::ErrorCode::NotFound, "clip to unlink not found");
        }
    }

    std::unordered_set<model::ClipId> to_unlink;
    if (!ignore_links) {
        for (const auto& cid : clip_ids) {
            const auto [trk, cl] = st.find_clip(cid);
            if (cl->link_id.has_value()) {
                const auto lid = *cl->link_id;
                for (const auto& t : st.tracks()) {
                    for (const auto& other_cl : t.clips) {
                        if (other_cl.link_id == lid) {
                            to_unlink.insert(other_cl.id);
                        }
                    }
                }
            }
        }
    } else {
        for (const auto& cid : clip_ids) {
            const auto [trk, cl] = st.find_clip(cid);
            if (cl->link_id.has_value()) {
                to_unlink.insert(cid);
            }
        }
    }

    if (to_unlink.empty()) {
        return ChangeSet{label(), {}};
    }

    for (const auto& cid : to_unlink) {
        const auto [trk, cl] = st.find_clip(cid);
        if (trk->locked) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "cannot unlink clip on locked track");
        }
    }

    for (const auto& cid : to_unlink) {
        auto [trk, cl] = st.find_clip(cid);
        st.touched_links().insert(*cl->link_id);
        cl->link_id = std::nullopt;
        trk->touched = true;
    }

    const auto repair_status = st.repair_links();
    if (!repair_status.has_value()) {
        return tl::unexpected(repair_status.error());
    }

    return st.diff_to_changes(label());
}

}  // namespace nxtcut::commands
