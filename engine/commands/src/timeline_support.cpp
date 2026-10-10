#include "timeline_support.hpp"

#include <nxtcut/core/error.hpp>
#include <nxtcut/core/frame_time.hpp>
#include <nxtcut/core/mul_div.hpp>
#include <nxtcut/model/checked_arithmetic.hpp>
#include <nxtcut/model/compound_graph.hpp>
#include <nxtcut/model/equality.hpp>
#include <nxtcut/model/speed.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

namespace nxtcut::commands::detail {

void fit_audio_fades(model::Clip& clip) noexcept {
    if (model::kind_of(clip.content) != model::ClipKind::Audio) {
        return;
    }
    auto& audio = std::get<model::AudioContent>(clip.content);
    if (audio.fade_in > clip.duration) {
        audio.fade_in = clip.duration;
    }
    const auto max_fade_out =
        core::Duration::from_ticks(clip.duration.ticks() - audio.fade_in.ticks());
    if (audio.fade_out > max_fade_out) {
        audio.fade_out = max_fade_out;
    }
}

core::Result<std::int64_t> source_offset_for_delta(const model::Speed& speed,
                                                   core::Duration delta) noexcept {
    const std::int64_t delta_ticks = delta.ticks();
    if (delta_ticks == 0) {
        return 0;
    }
    if (delta_ticks == std::numeric_limits<std::int64_t>::min()) {
        return core::make_error(core::ErrorCode::Overflow, "delta overflow");
    }

    const std::int64_t abs_delta = (delta_ticks < 0) ? -delta_ticks : delta_ticks;
    const auto scaled_res =
        core::mul_div(abs_delta, speed.numerator(), speed.denominator(), core::RoundingMode::Floor);
    if (!scaled_res.has_value()) {
        return tl::unexpected(scaled_res.error());
    }

    const std::int64_t scaled = *scaled_res;
    if (delta_ticks < 0) {
        return -scaled;
    }
    return scaled;
}

core::Status advance_source_in(model::Clip& clip, core::Duration timeline_delta) noexcept {
    const model::ClipKind k = model::kind_of(clip.content);
    if (k == model::ClipKind::Image || k == model::ClipKind::Text) {
        clip.source_in = model::SourceTime::zero();
        return core::Status{};
    }

    const auto offset_res = source_offset_for_delta(clip.speed, timeline_delta);
    if (!offset_res.has_value()) {
        return tl::unexpected(offset_res.error());
    }

    const auto new_source_ticks = model::detail::checked_add(clip.source_in.ticks(), *offset_res);
    if (!new_source_ticks.has_value()) {
        return core::make_error(core::ErrorCode::Overflow, "source range arithmetic overflow");
    }

    if (*new_source_ticks < 0) {
        return core::make_error(core::ErrorCode::InvalidArgument, "source extends before zero");
    }

    clip.source_in = model::SourceTime::from_ticks(*new_source_ticks);
    return core::Status{};
}

core::Status stretch_to_duration(model::Clip& clip, core::Duration new_duration) noexcept {
    const model::ClipKind k = model::kind_of(clip.content);
    if (k == model::ClipKind::Image || k == model::ClipKind::Text) {
        return core::make_error(core::ErrorCode::InvalidArgument,
                                "cannot rate stretch image or text clips");
    }

    if (new_duration.ticks() <= 0) {
        return core::make_error(core::ErrorCode::InvalidArgument, "clip duration must be positive");
    }

    const auto span_res = model::source_span(clip);
    if (!span_res.has_value()) {
        return tl::unexpected(span_res.error());
    }

    const std::int64_t s_ticks = span_res.value().ticks();
    const std::int64_t d_ticks = new_duration.ticks();

    const auto speed_res = model::Speed::create(s_ticks, d_ticks);
    if (!speed_res.has_value()) {
        return tl::unexpected(speed_res.error());
    }
    const auto new_speed = speed_res.value();

    const std::int64_t n = new_speed.numerator();
    const std::int64_t d = new_speed.denominator();

    // Check speed limits: 1/100 <= n/d <= 100
    // Check n * 100 >= d (minimum speed limit 1/100)
    const auto n_times_100_res = core::mul_div(n, kMaxSpeedFactor, 1, core::RoundingMode::Floor);
    if (n_times_100_res.has_value()) {
        if (n_times_100_res.value() < d) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "clip speed below minimum limit (1/100)");
        }
    } else if (n_times_100_res.error().code() == core::ErrorCode::Overflow) {
        // Overflow means n * 100 is huge, inequality is satisfied.
    } else {
        return tl::unexpected(n_times_100_res.error());
    }

    // Check n <= 100 * d (maximum speed limit 100/1)
    const auto d_times_100_res = core::mul_div(d, kMaxSpeedFactor, 1, core::RoundingMode::Floor);
    if (d_times_100_res.has_value()) {
        if (n > d_times_100_res.value()) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "clip speed exceeds maximum limit (100x)");
        }
    } else if (d_times_100_res.error().code() == core::ErrorCode::Overflow) {
        // Overflow means 100 * d is huge, inequality is satisfied.
    } else {
        return tl::unexpected(d_times_100_res.error());
    }

    clip.speed = new_speed;
    clip.duration = new_duration;
    fit_audio_fades(clip);

    return core::Status{};
}

core::Result<model::TimelineTime> add_time(model::TimelineTime t, core::Duration d) noexcept {
    const auto sum = model::detail::checked_add(t.ticks(), d.ticks());
    if (!sum.has_value()) {
        return core::make_error(core::ErrorCode::Overflow, "time arithmetic overflow");
    }
    return model::TimelineTime::from_ticks(*sum);
}

core::Result<model::TimelineTime> sub_time(model::TimelineTime t, core::Duration d) noexcept {
    const auto diff = model::detail::checked_sub(t.ticks(), d.ticks());
    if (!diff.has_value()) {
        return core::make_error(core::ErrorCode::Overflow, "time arithmetic overflow");
    }
    return model::TimelineTime::from_ticks(*diff);
}

core::Result<core::Duration> diff_time(model::TimelineTime a, model::TimelineTime b) noexcept {
    const auto diff = model::detail::checked_sub(a.ticks(), b.ticks());
    if (!diff.has_value()) {
        return core::make_error(core::ErrorCode::Overflow, "time arithmetic overflow");
    }
    return core::Duration::from_ticks(*diff);
}

model::TimelineTime end_of(const model::Clip& clip) noexcept {
    const auto end = model::clip_end(clip);
    if (end.has_value()) {
        return *end;
    }
    return model::TimelineTime::from_ticks(std::numeric_limits<std::int64_t>::max());
}

core::Result<ScratchTimeline> ScratchTimeline::create(const model::Project& project,
                                                      model::SequenceId seq_id,
                                                      core::UuidGenerator& ids) {
    auto seq_it = project.sequences.find(seq_id);
    if (seq_it == project.sequences.end()) {
        return core::make_error(core::ErrorCode::NotFound, "sequence not found");
    }

    ScratchTimeline st(project, seq_id, seq_it->second, ids);
    st.tracks_.reserve(seq_it->second.tracks.size());
    for (std::size_t i = 0; i < seq_it->second.tracks.size(); ++i) {
        const auto& trk = seq_it->second.tracks[i];
        ScratchTrack st_track;
        st_track.index = i;
        st_track.id = trk.id;
        st_track.kind = trk.kind;
        st_track.locked = trk.locked;
        st_track.original_clips = trk.clips;
        st_track.clips = trk.clips;
        st_track.touched = false;
        st.tracks_.push_back(std::move(st_track));
    }

    return st;
}

ScratchTrack* ScratchTimeline::find_track(model::TrackId track_id) noexcept {
    for (auto& t : tracks_) {
        if (t.id == track_id) {
            return &t;
        }
    }
    return nullptr;
}

const ScratchTrack* ScratchTimeline::find_track(model::TrackId track_id) const noexcept {
    for (const auto& t : tracks_) {
        if (t.id == track_id) {
            return &t;
        }
    }
    return nullptr;
}

std::pair<ScratchTrack*, model::Clip*> ScratchTimeline::find_clip(model::ClipId clip_id) noexcept {
    for (auto& t : tracks_) {
        for (auto& c : t.clips) {
            if (c.id == clip_id) {
                return {&t, &c};
            }
        }
    }
    return {nullptr, nullptr};
}

std::pair<const ScratchTrack*, const model::Clip*> ScratchTimeline::find_clip(
    model::ClipId clip_id) const noexcept {
    for (const auto& t : tracks_) {
        for (const auto& c : t.clips) {
            if (c.id == clip_id) {
                return {&t, &c};
            }
        }
    }
    return {nullptr, nullptr};
}

core::Result<model::TimelineTime> ScratchTimeline::snap_time(model::TimelineTime t) const noexcept {
    const auto res =
        core::snap_to_frame(t.to_core(), sequence_->frame_rate, core::RoundingMode::Nearest);
    if (!res.has_value()) {
        return tl::unexpected(res.error());
    }
    return model::TimelineTime(res.value());
}

core::Result<core::Duration> ScratchTimeline::snap_duration(core::Duration d) const noexcept {
    const auto frame_dur_res = core::frame_duration(sequence_->frame_rate);
    if (!frame_dur_res.has_value()) {
        return tl::unexpected(frame_dur_res.error());
    }

    const auto res = core::snap_to_frame(core::TimePoint::from_ticks(d.ticks()),
                                         sequence_->frame_rate, core::RoundingMode::Nearest);
    if (!res.has_value()) {
        return tl::unexpected(res.error());
    }

    const core::Duration snapped = core::Duration::from_ticks(res.value().ticks());
    if (snapped < frame_dur_res.value()) {
        return core::make_error(core::ErrorCode::InvalidArgument,
                                "duration snaps to less than one frame");
    }
    return snapped;
}

core::Result<core::Duration> ScratchTimeline::snap_delta(core::Duration d) const noexcept {
    const auto res = core::snap_to_frame(core::TimePoint::from_ticks(d.ticks()),
                                         sequence_->frame_rate, core::RoundingMode::Nearest);
    if (!res.has_value()) {
        return tl::unexpected(res.error());
    }
    return core::Duration::from_ticks(res.value().ticks());
}

bool ScratchTimeline::is_clip_compatible(model::TrackKind track_kind,
                                         const model::ClipContent& content) const noexcept {
    const model::ClipKind c_kind = model::kind_of(content);
    if (track_kind == model::TrackKind::Video) {
        return (c_kind == model::ClipKind::Video || c_kind == model::ClipKind::Image ||
                c_kind == model::ClipKind::Text || c_kind == model::ClipKind::Compound);
    }
    return (c_kind == model::ClipKind::Audio);
}

core::Status ScratchTimeline::validate_clip_media_and_source(const model::Clip& clip) const {
    const auto check_source_fits = [](const model::Clip& cl,
                                      std::int64_t limit_ticks) -> core::Status {
        if (cl.source_in.ticks() < 0) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "clip source_in cannot be negative");
        }
        const auto span_res = model::source_span(cl);
        if (!span_res.has_value()) {
            return tl::unexpected(span_res.error());
        }
        const auto source_end =
            model::detail::checked_add(cl.source_in.ticks(), span_res.value().ticks());
        if (!source_end.has_value()) {
            return core::make_error(core::ErrorCode::Overflow, "source range arithmetic overflow");
        }
        if (*source_end > limit_ticks) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "source range exceeds media duration");
        }
        return core::Status{};
    };

    return std::visit(
        [&](const auto& c) -> core::Status {
            using T = std::decay_t<decltype(c)>;
            if constexpr (std::is_same_v<T, model::VideoContent>) {
                const auto* asset = model::find_media(project_, c.media);
                if (asset == nullptr) {
                    return core::make_error(core::ErrorCode::InvalidArgument,
                                            "referenced media asset not found");
                }
                if (asset->kind != model::MediaKind::Video || !asset->video.has_value()) {
                    return core::make_error(core::ErrorCode::InvalidArgument,
                                            "video clip requires video media asset");
                }
                return check_source_fits(clip, asset->duration.ticks());
            } else if constexpr (std::is_same_v<T, model::AudioContent>) {
                const auto* asset = model::find_media(project_, c.media);
                if (asset == nullptr) {
                    return core::make_error(core::ErrorCode::InvalidArgument,
                                            "referenced media asset not found");
                }
                const bool audio_ok =
                    (asset->kind == model::MediaKind::Audio) ||
                    (asset->kind == model::MediaKind::Video && asset->audio.has_value());
                if (!audio_ok) {
                    return core::make_error(core::ErrorCode::InvalidArgument,
                                            "audio clip requires audio stream info");
                }
                return check_source_fits(clip, asset->duration.ticks());
            } else if constexpr (std::is_same_v<T, model::ImageContent>) {
                const auto* asset = model::find_media(project_, c.media);
                if (asset == nullptr) {
                    return core::make_error(core::ErrorCode::InvalidArgument,
                                            "referenced media asset not found");
                }
                if (asset->kind != model::MediaKind::Image) {
                    return core::make_error(core::ErrorCode::InvalidArgument,
                                            "image clip requires image media asset");
                }
                if (clip.source_in.ticks() != 0 || clip.speed != model::Speed::normal()) {
                    return core::make_error(core::ErrorCode::InvalidArgument,
                                            "image clip must have source_in zero and 1/1 speed");
                }
                return core::Status{};
            } else if constexpr (std::is_same_v<T, model::TextContent>) {
                if (clip.source_in.ticks() != 0 || clip.speed != model::Speed::normal()) {
                    return core::make_error(core::ErrorCode::InvalidArgument,
                                            "text clip must have source_in zero and 1/1 speed");
                }
                return core::Status{};
            } else {
                static_assert(std::is_same_v<T, model::CompoundContent>);
                const auto* nested_seq = model::find_sequence(project_, c.sequence);
                if (nested_seq == nullptr) {
                    return core::make_error(core::ErrorCode::InvalidArgument,
                                            "referenced compound sequence not found");
                }
                if (model::would_create_cycle(project_, sequence_id_, c.sequence)) {
                    return core::make_error(core::ErrorCode::InvalidArgument,
                                            "compound clip creates cycle");
                }
                const auto dur_res = model::sequence_duration(*nested_seq);
                if (!dur_res.has_value()) {
                    return tl::unexpected(dur_res.error());
                }
                return check_source_fits(clip, dur_res.value().ticks());
            }
        },
        clip.content);
}

std::vector<model::ClipId> ScratchTimeline::expand_links(
    const std::vector<model::ClipId>& clip_ids) const {
    std::vector<model::ClipId> expanded = clip_ids;
    for (std::size_t i = 0; i < expanded.size(); ++i) {
        const auto [trk, cl] = find_clip(expanded[i]);
        if (cl != nullptr && cl->link_id.has_value()) {
            const auto lid = *cl->link_id;
            for (const auto& t : tracks_) {
                for (const auto& other_cl : t.clips) {
                    if (other_cl.link_id.has_value() && *other_cl.link_id == lid) {
                        if (std::find(expanded.begin(), expanded.end(), other_cl.id) ==
                            expanded.end()) {
                            expanded.push_back(other_cl.id);
                        }
                    }
                }
            }
        }
    }
    return expanded;
}

core::Result<std::pair<model::Clip, model::Clip>> ScratchTimeline::split_clip(
    const model::Clip& clip, model::TimelineTime at) {
    const auto end_res = model::clip_end(clip);
    if (!end_res.has_value()) {
        return tl::unexpected(end_res.error());
    }

    if (at <= clip.start || at >= *end_res) {
        return core::make_error(core::ErrorCode::InvalidArgument,
                                "split time must be strictly inside clip");
    }

    const auto left_dur_res = diff_time(at, clip.start);
    if (!left_dur_res.has_value()) {
        return tl::unexpected(left_dur_res.error());
    }

    const auto right_dur_res = diff_time(*end_res, at);
    if (!right_dur_res.has_value()) {
        return tl::unexpected(right_dur_res.error());
    }

    model::Clip left = clip;
    left.duration = *left_dur_res;

    model::Clip right = clip;
    right.id = model::generate_id<model::ClipId>(ids_);
    right.start = at;
    right.duration = *right_dur_res;
    right.link_id = std::nullopt;

    const model::ClipKind c_kind = model::kind_of(clip.content);
    if (c_kind == model::ClipKind::Image || c_kind == model::ClipKind::Text) {
        right.source_in = model::SourceTime::zero();
    } else {
        const auto c_offset = model::ClipTime::from_ticks(at.ticks() - clip.start.ticks());
        const auto source_res = model::clip_to_source(clip, c_offset);
        if (!source_res.has_value()) {
            return tl::unexpected(source_res.error());
        }
        right.source_in = source_res.value();
    }

    if (c_kind == model::ClipKind::Audio) {
        auto& left_audio = std::get<model::AudioContent>(left.content);
        left_audio.fade_out = core::Duration::zero();

        auto& right_audio = std::get<model::AudioContent>(right.content);
        right_audio.fade_in = core::Duration::zero();
    }
    fit_audio_fades(left);
    fit_audio_fades(right);

    if (clip.link_id.has_value()) {
        touched_links_.insert(*clip.link_id);
    }

    return std::pair{std::move(left), std::move(right)};
}

core::Status ScratchTimeline::trim_head(model::Clip& clip, model::TimelineTime new_start) {
    const auto end_res = model::clip_end(clip);
    if (!end_res.has_value()) {
        return tl::unexpected(end_res.error());
    }

    if (new_start >= *end_res) {
        return core::make_error(core::ErrorCode::InvalidArgument,
                                "head trim must be before clip end");
    }

    const auto old_start = clip.start;
    const auto new_dur_res = diff_time(*end_res, new_start);
    if (!new_dur_res.has_value()) {
        return tl::unexpected(new_dur_res.error());
    }

    const auto delta_res = diff_time(new_start, old_start);
    if (!delta_res.has_value()) {
        return tl::unexpected(delta_res.error());
    }

    clip.start = new_start;
    clip.duration = *new_dur_res;

    const auto adv_status = advance_source_in(clip, *delta_res);
    if (!adv_status.has_value()) {
        return adv_status;
    }

    fit_audio_fades(clip);

    if (clip.link_id.has_value()) {
        touched_links_.insert(*clip.link_id);
    }

    return core::Status{};
}

core::Status ScratchTimeline::trim_tail(model::Clip& clip, model::TimelineTime new_end) {
    if (new_end <= clip.start) {
        return core::make_error(core::ErrorCode::InvalidArgument,
                                "tail trim must be after clip start");
    }

    const auto new_dur_res = diff_time(new_end, clip.start);
    if (!new_dur_res.has_value()) {
        return tl::unexpected(new_dur_res.error());
    }

    clip.duration = *new_dur_res;

    fit_audio_fades(clip);

    if (clip.link_id.has_value()) {
        touched_links_.insert(*clip.link_id);
    }

    return core::Status{};
}

core::Status ScratchTimeline::check_overlaps_on_touched_tracks() const {
    for (const auto& track : tracks_) {
        if (!track.touched) {
            continue;
        }

        struct ClipSpan {
            std::int64_t start;
            std::int64_t end;
        };
        std::vector<ClipSpan> spans;
        spans.reserve(track.clips.size());

        for (const auto& clip : track.clips) {
            if (clip.start.ticks() < 0) {
                return core::make_error(core::ErrorCode::InvalidArgument,
                                        "clip start cannot be negative");
            }
            if (clip.duration.ticks() <= 0) {
                return core::make_error(core::ErrorCode::InvalidArgument,
                                        "clip duration must be positive");
            }
            const auto end_res = model::clip_end(clip);
            if (!end_res.has_value()) {
                return tl::unexpected(end_res.error());
            }
            spans.push_back(ClipSpan{clip.start.ticks(), end_res.value().ticks()});
        }

        std::stable_sort(spans.begin(), spans.end(),
                         [](const ClipSpan& a, const ClipSpan& b) { return a.start < b.start; });

        std::int64_t max_end = std::numeric_limits<std::int64_t>::min();
        for (std::size_t i = 0; i < spans.size(); ++i) {
            if (i > 0 && spans[i].start < max_end) {
                return core::make_error(core::ErrorCode::InvalidArgument,
                                        "clip overlap detected on track");
            }
            if (spans[i].end > max_end) {
                max_end = spans[i].end;
            }
        }
    }
    return core::Status{};
}

core::Status ScratchTimeline::repair_links() {
    for (const auto& lid : touched_links_) {
        std::size_t count = 0;
        for (const auto& trk : tracks_) {
            for (const auto& cl : trk.clips) {
                if (cl.link_id.has_value() && *cl.link_id == lid) {
                    ++count;
                }
            }
        }

        if (count > 0 && count < 2) {
            for (auto& trk : tracks_) {
                for (auto& cl : trk.clips) {
                    if (cl.link_id.has_value() && *cl.link_id == lid) {
                        if (trk.locked) {
                            return core::make_error(core::ErrorCode::InvalidArgument,
                                                    "cannot repair link on locked track");
                        }
                        cl.link_id = std::nullopt;
                        trk.touched = true;
                    }
                }
            }
        }
    }
    return core::Status{};
}

core::Result<ChangeSet> ScratchTimeline::diff_to_changes(const std::string& label) const {
    ChangeSet cs;
    cs.label = label;

    struct ChangeItem {
        std::size_t track_index;
        std::int64_t start_ticks;
        model::ClipId id;
        ClipChange change;
    };
    std::vector<ChangeItem> all_changes;

    for (const auto& track : tracks_) {
        if (!track.touched) {
            continue;
        }

        // 1. Removals and modifications of original clips
        for (const auto& orig : track.original_clips) {
            auto it = std::find_if(track.clips.begin(), track.clips.end(),
                                   [&orig](const model::Clip& c) { return c.id == orig.id; });
            if (it == track.clips.end()) {
                // Removal
                ClipChange ch{sequence_id_, track.id, orig.id, orig, std::nullopt};
                all_changes.push_back(
                    ChangeItem{track.index, orig.start.ticks(), orig.id, std::move(ch)});
            } else if (!model::identical(orig, *it)) {
                // Modification
                ClipChange ch{sequence_id_, track.id, orig.id, orig, *it};
                all_changes.push_back(
                    ChangeItem{track.index, orig.start.ticks(), orig.id, std::move(ch)});
            }
        }

        // 2. Additions of new clips
        for (const auto& cur : track.clips) {
            auto it = std::find_if(track.original_clips.begin(), track.original_clips.end(),
                                   [&cur](const model::Clip& c) { return c.id == cur.id; });
            if (it == track.original_clips.end()) {
                // Addition
                ClipChange ch{sequence_id_, track.id, cur.id, std::nullopt, cur};
                all_changes.push_back(
                    ChangeItem{track.index, cur.start.ticks(), cur.id, std::move(ch)});
            }
        }
    }

    std::stable_sort(all_changes.begin(), all_changes.end(),
                     [](const ChangeItem& a, const ChangeItem& b) {
                         if (a.track_index != b.track_index) {
                             return a.track_index < b.track_index;
                         }
                         if (a.start_ticks != b.start_ticks) {
                             return a.start_ticks < b.start_ticks;
                         }
                         return a.id < b.id;
                     });

    cs.changes.reserve(all_changes.size());
    for (auto& item : all_changes) {
        cs.changes.push_back(std::move(item.change));
    }

    return cs;
}

}  // namespace nxtcut::commands::detail
