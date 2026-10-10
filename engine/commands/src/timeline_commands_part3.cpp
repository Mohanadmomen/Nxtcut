#include <nxtcut/commands/timeline_commands.hpp>
#include <nxtcut/core/error.hpp>
#include <nxtcut/core/frame_time.hpp>
#include <nxtcut/model/clip.hpp>
#include <nxtcut/model/ids.hpp>

#include <algorithm>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "timeline_support.hpp"

namespace nxtcut::commands {
namespace {

[[nodiscard]] core::Status validate_at_least_one_frame(core::Duration dur,
                                                       core::Duration frame_dur) noexcept {
    if (dur < frame_dur) {
        return core::make_error(core::ErrorCode::InvalidArgument,
                                "clip duration less than one frame");
    }
    return core::Status{};
}

}  // namespace

// ============================================================================
// RollEdit
// ============================================================================

core::Result<ChangeSet> RollEdit::build(const model::Project& project,
                                        core::UuidGenerator& ids) const {
    auto st_res = detail::ScratchTimeline::create(project, sequence, ids);
    if (!st_res.has_value()) {
        return tl::unexpected(st_res.error());
    }
    auto& st = st_res.value();

    const auto [left_trk, left_clip] = st.find_clip(left);
    const auto [right_trk, right_clip] = st.find_clip(right);
    if (left_clip == nullptr || left_trk == nullptr || right_clip == nullptr ||
        right_trk == nullptr) {
        return core::make_error(core::ErrorCode::NotFound, "clip for roll edit not found");
    }

    if (left == right || left_trk != right_trk || detail::end_of(*left_clip) != right_clip->start) {
        return core::make_error(core::ErrorCode::InvalidArgument,
                                "roll edit requires adjacent clips on the same track");
    }

    if (!ignore_links && left_clip->link_id.has_value() && right_clip->link_id.has_value() &&
        *left_clip->link_id == *right_clip->link_id) {
        return core::make_error(core::ErrorCode::InvalidArgument,
                                "left and right clips share the same link");
    }

    if (left_trk->locked) {
        return core::make_error(core::ErrorCode::InvalidArgument,
                                "cannot roll edit on locked track");
    }

    // Collect left and right link targets
    std::vector<model::ClipId> left_targets = {left};
    std::vector<model::ClipId> right_targets = {right};

    if (!ignore_links) {
        if (left_clip->link_id.has_value()) {
            const auto lid = *left_clip->link_id;
            for (const auto& trk : st.tracks()) {
                for (const auto& cl : trk.clips) {
                    if (cl.link_id.has_value() && *cl.link_id == lid && cl.id != left) {
                        if (trk.locked) {
                            return core::make_error(core::ErrorCode::InvalidArgument,
                                                    "cannot roll edit linked clip on locked track");
                        }
                        left_targets.push_back(cl.id);
                    }
                }
            }
        }
        if (right_clip->link_id.has_value()) {
            const auto lid = *right_clip->link_id;
            for (const auto& trk : st.tracks()) {
                for (const auto& cl : trk.clips) {
                    if (cl.link_id.has_value() && *cl.link_id == lid && cl.id != right) {
                        if (trk.locked) {
                            return core::make_error(core::ErrorCode::InvalidArgument,
                                                    "cannot roll edit linked clip on locked track");
                        }
                        right_targets.push_back(cl.id);
                    }
                }
            }
        }
    }

    const auto snapped_edit_res = st.snap_time(new_edit);
    if (!snapped_edit_res.has_value()) {
        return tl::unexpected(snapped_edit_res.error());
    }
    const auto snapped_edit = snapped_edit_res.value();

    const auto delta_res = detail::diff_time(snapped_edit, detail::end_of(*left_clip));
    if (!delta_res.has_value()) {
        return tl::unexpected(delta_res.error());
    }
    const auto delta = *delta_res;

    if (delta.ticks() == 0) {
        return ChangeSet{label(), {}};
    }

    const auto frame_dur_res = core::frame_duration(st.sequence().frame_rate);
    if (!frame_dur_res.has_value()) {
        return tl::unexpected(frame_dur_res.error());
    }
    const auto frame_dur = frame_dur_res.value();

    // Verify minimum 1 frame duration for all left and right targets
    for (const auto& lid : left_targets) {
        const auto [trk, cl] = st.find_clip(lid);
        static_cast<void>(trk);
        const auto cl_end = detail::end_of(*cl);
        const auto new_end_res = detail::add_time(cl_end, delta);
        if (!new_end_res.has_value()) {
            return tl::unexpected(new_end_res.error());
        }
        if (*new_end_res <= cl->start) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "clip duration less than one frame");
        }
        const auto new_dur_res = detail::diff_time(*new_end_res, cl->start);
        if (!new_dur_res.has_value()) {
            return tl::unexpected(new_dur_res.error());
        }
        const auto val_status = validate_at_least_one_frame(*new_dur_res, frame_dur);
        if (!val_status.has_value()) {
            return tl::unexpected(val_status.error());
        }
    }

    for (const auto& rid : right_targets) {
        const auto [trk, cl] = st.find_clip(rid);
        static_cast<void>(trk);
        const auto cl_end = detail::end_of(*cl);
        const auto new_start_res = detail::add_time(cl->start, delta);
        if (!new_start_res.has_value()) {
            return tl::unexpected(new_start_res.error());
        }
        if (*new_start_res >= cl_end) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "clip duration less than one frame");
        }
        const auto new_dur_res = detail::diff_time(cl_end, *new_start_res);
        if (!new_dur_res.has_value()) {
            return tl::unexpected(new_dur_res.error());
        }
        const auto val_status = validate_at_least_one_frame(*new_dur_res, frame_dur);
        if (!val_status.has_value()) {
            return tl::unexpected(val_status.error());
        }
    }

    // Apply tail trim to all left targets
    for (const auto& lid : left_targets) {
        auto [trk, cl] = st.find_clip(lid);
        const auto cl_end = detail::end_of(*cl);
        const auto new_end_res = detail::add_time(cl_end, delta);
        auto trim_status = st.trim_tail(*cl, new_end_res.value());
        if (!trim_status.has_value()) {
            return tl::unexpected(trim_status.error());
        }
        auto val_status = st.validate_clip_media_and_source(*cl);
        if (!val_status.has_value()) {
            return tl::unexpected(val_status.error());
        }
        trk->touched = true;
    }

    // Apply head trim to all right targets
    for (const auto& rid : right_targets) {
        auto [trk, cl] = st.find_clip(rid);
        const auto new_start_res = detail::add_time(cl->start, delta);
        auto trim_status = st.trim_head(*cl, new_start_res.value());
        if (!trim_status.has_value()) {
            return tl::unexpected(trim_status.error());
        }
        auto val_status = st.validate_clip_media_and_source(*cl);
        if (!val_status.has_value()) {
            return tl::unexpected(val_status.error());
        }
        trk->touched = true;
    }

    const auto overlap_status = st.check_overlaps_on_touched_tracks();
    if (!overlap_status.has_value()) {
        return tl::unexpected(overlap_status.error());
    }

    return st.diff_to_changes(label());
}

// ============================================================================
// SlipClip
// ============================================================================

core::Result<ChangeSet> SlipClip::build(const model::Project& project,
                                        core::UuidGenerator& ids) const {
    auto st_res = detail::ScratchTimeline::create(project, sequence, ids);
    if (!st_res.has_value()) {
        return tl::unexpected(st_res.error());
    }
    auto& st = st_res.value();

    const auto [orig_trk, orig_clip] = st.find_clip(clip);
    if (orig_clip == nullptr || orig_trk == nullptr) {
        return core::make_error(core::ErrorCode::NotFound, "clip for slip edit not found");
    }

    if (orig_trk->locked) {
        return core::make_error(core::ErrorCode::InvalidArgument,
                                "cannot slip clip on locked track");
    }

    const auto snapped_delta_res = st.snap_delta(delta);
    if (!snapped_delta_res.has_value()) {
        return tl::unexpected(snapped_delta_res.error());
    }
    const auto snapped_delta = snapped_delta_res.value();

    if (snapped_delta.ticks() == 0) {
        return ChangeSet{label(), {}};
    }

    // Collect slip targets
    std::vector<model::ClipId> targets = {clip};
    if (!ignore_links && orig_clip->link_id.has_value()) {
        const auto lid = *orig_clip->link_id;
        for (const auto& trk : st.tracks()) {
            for (const auto& cl : trk.clips) {
                if (cl.link_id.has_value() && *cl.link_id == lid && cl.id != clip) {
                    if (trk.locked) {
                        return core::make_error(core::ErrorCode::InvalidArgument,
                                                "cannot slip linked clip on locked track");
                    }
                    targets.push_back(cl.id);
                }
            }
        }
    }

    // Disallow Image and Text clips from slipping
    for (const auto& tid : targets) {
        const auto [trk, cl] = st.find_clip(tid);
        static_cast<void>(trk);
        const auto k = model::kind_of(cl->content);
        if (k == model::ClipKind::Image || k == model::ClipKind::Text) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "cannot slip image or text clip");
        }
    }

    // Apply slip to each target
    for (const auto& tid : targets) {
        auto [trk, cl] = st.find_clip(tid);
        const auto adv_status = detail::advance_source_in(*cl, snapped_delta);
        if (!adv_status.has_value()) {
            return tl::unexpected(adv_status.error());
        }
        const auto val_status = st.validate_clip_media_and_source(*cl);
        if (!val_status.has_value()) {
            return tl::unexpected(val_status.error());
        }
        trk->touched = true;
    }

    const auto overlap_status = st.check_overlaps_on_touched_tracks();
    if (!overlap_status.has_value()) {
        return tl::unexpected(overlap_status.error());
    }

    return st.diff_to_changes(label());
}

// ============================================================================
// SlideClip
// ============================================================================

core::Result<ChangeSet> SlideClip::build(const model::Project& project,
                                         core::UuidGenerator& ids) const {
    auto st_res = detail::ScratchTimeline::create(project, sequence, ids);
    if (!st_res.has_value()) {
        return tl::unexpected(st_res.error());
    }
    auto& st = st_res.value();

    const auto [orig_trk, orig_clip] = st.find_clip(clip);
    if (orig_clip == nullptr || orig_trk == nullptr) {
        return core::make_error(core::ErrorCode::NotFound, "clip for slide edit not found");
    }

    if (orig_trk->locked) {
        return core::make_error(core::ErrorCode::InvalidArgument,
                                "cannot slide clip on locked track");
    }

    // Collect slid targets
    std::vector<model::ClipId> slid_targets = {clip};
    if (!ignore_links && orig_clip->link_id.has_value()) {
        const auto lid = *orig_clip->link_id;
        for (const auto& trk : st.tracks()) {
            for (const auto& cl : trk.clips) {
                if (cl.link_id.has_value() && *cl.link_id == lid && cl.id != clip) {
                    if (trk.locked) {
                        return core::make_error(core::ErrorCode::InvalidArgument,
                                                "cannot slide linked clip on locked track");
                    }
                    slid_targets.push_back(cl.id);
                }
            }
        }
    }

    struct SlideTrio {
        detail::ScratchTrack* track{nullptr};
        model::Clip* left{nullptr};
        model::Clip* slid{nullptr};
        model::Clip* right{nullptr};
    };
    std::vector<SlideTrio> trios;
    trios.reserve(slid_targets.size());

    enum class Role { Slid, LeftNeighbor, RightNeighbor };
    std::unordered_map<model::ClipId, Role> clip_roles;

    // Register slid clips first
    for (const auto& sid : slid_targets) {
        if (clip_roles.contains(sid)) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "ambiguous clip role in slide edit");
        }
        clip_roles[sid] = Role::Slid;
    }

    // Locate neighbors and verify role exclusivity
    for (const auto& sid : slid_targets) {
        auto [trk, s_clip] = st.find_clip(sid);
        if (trk->locked) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "cannot slide clip on locked track");
        }

        model::Clip* left_neighbor = nullptr;
        model::Clip* right_neighbor = nullptr;

        const auto s_end = detail::end_of(*s_clip);

        for (auto& candidate : trk->clips) {
            if (candidate.id == sid) {
                continue;
            }
            if (detail::end_of(candidate) == s_clip->start) {
                left_neighbor = &candidate;
            }
            if (candidate.start == s_end) {
                right_neighbor = &candidate;
            }
        }

        if (left_neighbor == nullptr) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "slid clip missing touching left neighbor");
        }
        if (right_neighbor == nullptr) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "slid clip missing touching right neighbor");
        }

        if (clip_roles.contains(left_neighbor->id)) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "ambiguous clip role in slide edit");
        }
        clip_roles[left_neighbor->id] = Role::LeftNeighbor;

        if (clip_roles.contains(right_neighbor->id)) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "ambiguous clip role in slide edit");
        }
        clip_roles[right_neighbor->id] = Role::RightNeighbor;

        trios.push_back(SlideTrio{trk, left_neighbor, s_clip, right_neighbor});
    }

    const auto snapped_start_res = st.snap_time(new_start);
    if (!snapped_start_res.has_value()) {
        return tl::unexpected(snapped_start_res.error());
    }
    const auto snapped_start = snapped_start_res.value();

    const auto delta_res = detail::diff_time(snapped_start, orig_clip->start);
    if (!delta_res.has_value()) {
        return tl::unexpected(delta_res.error());
    }
    const auto delta = *delta_res;

    if (delta.ticks() == 0) {
        return ChangeSet{label(), {}};
    }

    const auto frame_dur_res = core::frame_duration(st.sequence().frame_rate);
    if (!frame_dur_res.has_value()) {
        return tl::unexpected(frame_dur_res.error());
    }
    const auto frame_dur = frame_dur_res.value();

    // Verify minimum 1 frame duration for neighbors and non-negative start
    for (const auto& trio : trios) {
        // Left neighbor
        const auto l_end = detail::end_of(*trio.left);
        const auto l_new_end_res = detail::add_time(l_end, delta);
        if (!l_new_end_res.has_value()) {
            return tl::unexpected(l_new_end_res.error());
        }
        if (*l_new_end_res <= trio.left->start) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "neighbor duration less than one frame");
        }
        const auto l_new_dur_res = detail::diff_time(*l_new_end_res, trio.left->start);
        if (!l_new_dur_res.has_value()) {
            return tl::unexpected(l_new_dur_res.error());
        }
        const auto l_val_status = validate_at_least_one_frame(*l_new_dur_res, frame_dur);
        if (!l_val_status.has_value()) {
            return tl::unexpected(l_val_status.error());
        }

        // Slid clip
        const auto s_new_start_res = detail::add_time(trio.slid->start, delta);
        if (!s_new_start_res.has_value()) {
            return tl::unexpected(s_new_start_res.error());
        }
        if (s_new_start_res.value().ticks() < 0) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "clip start cannot be negative");
        }

        // Right neighbor
        const auto r_end = detail::end_of(*trio.right);
        const auto r_new_start_res = detail::add_time(trio.right->start, delta);
        if (!r_new_start_res.has_value()) {
            return tl::unexpected(r_new_start_res.error());
        }
        if (*r_new_start_res >= r_end) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "neighbor duration less than one frame");
        }
        const auto r_new_dur_res = detail::diff_time(r_end, *r_new_start_res);
        if (!r_new_dur_res.has_value()) {
            return tl::unexpected(r_new_dur_res.error());
        }
        const auto r_val_status = validate_at_least_one_frame(*r_new_dur_res, frame_dur);
        if (!r_val_status.has_value()) {
            return tl::unexpected(r_val_status.error());
        }
    }

    // Apply modifications to all trios
    for (auto& trio : trios) {
        // Left neighbor
        const auto l_end = detail::end_of(*trio.left);
        const auto l_new_end_res = detail::add_time(l_end, delta);
        auto l_trim_status = st.trim_tail(*trio.left, l_new_end_res.value());
        if (!l_trim_status.has_value()) {
            return tl::unexpected(l_trim_status.error());
        }

        // Slid clip
        const auto s_new_start_res = detail::add_time(trio.slid->start, delta);
        trio.slid->start = s_new_start_res.value();
        if (trio.slid->link_id.has_value()) {
            st.touched_links().insert(*trio.slid->link_id);
        }

        // Right neighbor
        const auto r_new_start_res = detail::add_time(trio.right->start, delta);
        auto r_trim_status = st.trim_head(*trio.right, r_new_start_res.value());
        if (!r_trim_status.has_value()) {
            return tl::unexpected(r_trim_status.error());
        }

        // Validate source bounds for all three
        auto l_val = st.validate_clip_media_and_source(*trio.left);
        if (!l_val.has_value()) {
            return tl::unexpected(l_val.error());
        }
        auto s_val = st.validate_clip_media_and_source(*trio.slid);
        if (!s_val.has_value()) {
            return tl::unexpected(s_val.error());
        }
        auto r_val = st.validate_clip_media_and_source(*trio.right);
        if (!r_val.has_value()) {
            return tl::unexpected(r_val.error());
        }

        trio.track->touched = true;
    }

    const auto overlap_status = st.check_overlaps_on_touched_tracks();
    if (!overlap_status.has_value()) {
        return tl::unexpected(overlap_status.error());
    }

    return st.diff_to_changes(label());
}

}  // namespace nxtcut::commands
