#include <nxtcut/core/error.hpp>
#include <nxtcut/model/checked_arithmetic.hpp>
#include <nxtcut/model/compound_graph.hpp>
#include <nxtcut/model/validation.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

namespace nxtcut::model {

namespace {

struct ValidClipSpan {
    std::size_t clip_idx;
    std::int64_t start;
    std::int64_t end;
};

struct LinkRef {
    SequenceId seq_key;
    std::string clip_path;
};

bool source_range_fits(const Clip& clip, std::int64_t limit_ticks) noexcept {
    if (clip.source_in.ticks() < 0) {
        return false;
    }
    const auto span_res = source_span(clip);
    if (!span_res.has_value()) {
        return false;
    }
    const auto source_end = detail::checked_add(clip.source_in.ticks(), span_res.value().ticks());
    if (!source_end.has_value()) {
        return false;
    }
    return *source_end <= limit_ticks;
}

class IssueCollector {
public:
    explicit IssueCollector(const Project& project) noexcept : project_(project) {}

    std::vector<ValidationIssue> collect() {
        check_project_level();
        check_media_map();
        for (const auto& [seq_key, seq] : project_.sequences) {
            check_sequence(seq_key, seq);
        }
        check_compound_cycles();
        check_links();
        return std::move(issues_);
    }

private:
    void check_project_level() {
        if (project_.sequences.find(project_.main_sequence) == project_.sequences.end()) {
            issues_.push_back(
                ValidationIssue{ValidationCode::MissingMainSequence, "main_sequence",
                                "Project main sequence does not exist in sequences map"});
        }
    }

    void check_media_map() {
        for (const auto& [media_key, asset] : project_.media) {
            if (media_key != asset.id) {
                issues_.push_back(ValidationIssue{ValidationCode::IdMismatch,
                                                  "media/" + media_key.to_string(),
                                                  "Media map key does not match asset ID"});
            }
        }
    }

    void check_sequence(const SequenceId& seq_key, const Sequence& seq) {
        const std::string seq_path = "sequences/" + seq_key.to_string();

        if (seq_key != seq.id) {
            issues_.push_back(ValidationIssue{ValidationCode::IdMismatch, seq_path,
                                              "Sequence map key does not match sequence ID"});
        }

        if (seq.canvas.width <= 0 || seq.canvas.height <= 0) {
            issues_.push_back(
                ValidationIssue{ValidationCode::InvalidCanvas, seq_path,
                                "Sequence canvas width and height must be strictly positive"});
        }

        for (std::size_t t_idx = 0; t_idx < seq.tracks.size(); ++t_idx) {
            check_track(seq_key, seq_path, t_idx, seq.tracks[t_idx]);
        }

        check_markers(seq_path, seq);
    }

    void check_track(const SequenceId& seq_key, const std::string& seq_path, std::size_t t_idx,
                     const Track& track) {
        const std::string track_path = seq_path + "/tracks/" + std::to_string(t_idx);

        if (!seen_track_ids_.insert(track.id).second) {
            issues_.push_back(ValidationIssue{ValidationCode::DuplicateId, track_path,
                                              "Duplicate TrackId found across project"});
        }

        std::vector<ValidClipSpan> track_valid_clips;
        for (std::size_t c_idx = 0; c_idx < track.clips.size(); ++c_idx) {
            check_clip(seq_key, track_path, c_idx, track, track.clips[c_idx], track_valid_clips);
        }

        check_track_overlaps(track_path, track_valid_clips);
    }

    void check_clip(const SequenceId& seq_key, const std::string& track_path, std::size_t c_idx,
                    const Track& track, const Clip& clip,
                    std::vector<ValidClipSpan>& track_valid_clips) {
        const std::string clip_path = track_path + "/clips/" + std::to_string(c_idx);

        if (!seen_clip_ids_.insert(clip.id).second) {
            issues_.push_back(ValidationIssue{ValidationCode::DuplicateId, clip_path,
                                              "Duplicate ClipId found across project"});
        }

        bool has_range_overflow = false;
        bool has_duration_error = false;

        if (clip.duration.ticks() <= 0) {
            issues_.push_back(ValidationIssue{ValidationCode::InvalidDuration, clip_path,
                                              "Clip duration must be strictly positive"});
            has_duration_error = true;
        }

        if (clip.start.ticks() < 0) {
            issues_.push_back(ValidationIssue{ValidationCode::NegativeStart, clip_path,
                                              "Clip start time must be non-negative"});
        }

        const auto end_res = clip_end(clip);
        if (!end_res.has_value()) {
            issues_.push_back(
                ValidationIssue{ValidationCode::ClipRangeOverflow, clip_path,
                                "Clip range (start + duration) overflows integer bounds"});
            has_range_overflow = true;
        }

        if (!has_range_overflow && !has_duration_error) {
            track_valid_clips.push_back(
                ValidClipSpan{c_idx, clip.start.ticks(), end_res.value().ticks()});
        }

        const ClipKind c_kind = kind_of(clip.content);
        bool kind_ok = false;
        if (track.kind == TrackKind::Video) {
            kind_ok = (c_kind == ClipKind::Video || c_kind == ClipKind::Image ||
                       c_kind == ClipKind::Text || c_kind == ClipKind::Compound);
        } else {
            kind_ok = (c_kind == ClipKind::Audio);
        }

        if (!kind_ok) {
            issues_.push_back(ValidationIssue{ValidationCode::ClipKindMismatch, clip_path,
                                              "Clip kind is incompatible with track kind"});
        }

        std::unordered_set<EffectId> seen_effect_ids;
        for (std::size_t e_idx = 0; e_idx < clip.effects.size(); ++e_idx) {
            if (!seen_effect_ids.insert(clip.effects[e_idx].id).second) {
                issues_.push_back(ValidationIssue{ValidationCode::DuplicateId,
                                                  clip_path + "/effects/" + std::to_string(e_idx),
                                                  "Duplicate EffectId found within clip"});
            }
        }

        if (!has_range_overflow && !has_duration_error) {
            check_clip_content(clip_path, clip);
        }

        if (clip.link_id.has_value()) {
            link_usage_[*clip.link_id].push_back(LinkRef{seq_key, clip_path});
        }
    }

    void check_clip_content(const std::string& clip_path, const Clip& clip) {
        std::visit([&](const auto& c) { check_content(clip_path, clip, c); }, clip.content);
    }

    void check_content(const std::string& clip_path, const Clip& clip,
                       const VideoContent& content) {
        const auto* asset = find_media(project_, content.media);
        if (asset == nullptr) {
            issues_.push_back(ValidationIssue{ValidationCode::MissingMedia, clip_path,
                                              "Referenced media asset not found in project"});
        } else if (asset->kind != MediaKind::Video || !asset->video.has_value()) {
            issues_.push_back(
                ValidationIssue{ValidationCode::MediaKindMismatch, clip_path,
                                "Video clip requires video media asset with video stream info"});
        } else if (!source_range_fits(clip, asset->duration.ticks())) {
            issues_.push_back(ValidationIssue{ValidationCode::SourceRangeOutOfBounds, clip_path,
                                              "Source range exceeds media duration"});
        }
    }

    void check_content(const std::string& clip_path, const Clip& clip,
                       const AudioContent& content) {
        const auto* asset = find_media(project_, content.media);
        if (asset == nullptr) {
            issues_.push_back(ValidationIssue{ValidationCode::MissingMedia, clip_path,
                                              "Referenced media asset not found in project"});
        } else {
            const bool audio_ok = (asset->kind == MediaKind::Audio) ||
                                  (asset->kind == MediaKind::Video && asset->audio.has_value());
            if (!audio_ok) {
                issues_.push_back(ValidationIssue{
                    ValidationCode::MediaKindMismatch, clip_path,
                    "Audio clip requires audio asset or video asset with audio stream info"});
            } else if (!source_range_fits(clip, asset->duration.ticks())) {
                issues_.push_back(ValidationIssue{ValidationCode::SourceRangeOutOfBounds, clip_path,
                                                  "Source range exceeds media duration"});
            }
        }
    }

    void check_content(const std::string& clip_path, const Clip& clip,
                       const ImageContent& content) {
        const auto* asset = find_media(project_, content.media);
        if (asset == nullptr) {
            issues_.push_back(ValidationIssue{ValidationCode::MissingMedia, clip_path,
                                              "Referenced media asset not found in project"});
        } else if (asset->kind != MediaKind::Image) {
            issues_.push_back(ValidationIssue{ValidationCode::MediaKindMismatch, clip_path,
                                              "Image clip requires image media asset"});
        } else if (clip.source_in.ticks() != 0 || clip.speed != Speed::normal()) {
            issues_.push_back(ValidationIssue{ValidationCode::SourceRangeOutOfBounds, clip_path,
                                              "Image clip must have source_in zero and 1/1 speed"});
        }
    }

    void check_content(const std::string& clip_path, const Clip& clip,
                       const TextContent& /*content*/) {
        if (clip.source_in.ticks() != 0 || clip.speed != Speed::normal()) {
            issues_.push_back(ValidationIssue{ValidationCode::SourceRangeOutOfBounds, clip_path,
                                              "Text clip must have source_in zero and 1/1 speed"});
        }
    }

    void check_content(const std::string& clip_path, const Clip& clip,
                       const CompoundContent& content) {
        const auto* nested_seq = find_sequence(project_, content.sequence);
        if (nested_seq == nullptr) {
            issues_.push_back(ValidationIssue{ValidationCode::MissingSequence, clip_path,
                                              "Compound clip references non-existent sequence"});
        } else {
            const auto dur_res = sequence_duration(*nested_seq);
            if (!dur_res.has_value() || !source_range_fits(clip, dur_res.value().ticks())) {
                issues_.push_back(ValidationIssue{ValidationCode::SourceRangeOutOfBounds, clip_path,
                                                  "Source range exceeds nested sequence duration"});
            }
        }
    }

    void check_track_overlaps(const std::string& track_path,
                              std::vector<ValidClipSpan>& track_valid_clips) {
        std::stable_sort(track_valid_clips.begin(), track_valid_clips.end(),
                         [](const auto& a, const auto& b) { return a.start < b.start; });

        std::int64_t max_end = std::numeric_limits<std::int64_t>::min();
        for (std::size_t i = 0; i < track_valid_clips.size(); ++i) {
            if (i > 0 && track_valid_clips[i].start < max_end) {
                const std::string overlap_path =
                    track_path + "/clips/" + std::to_string(track_valid_clips[i].clip_idx);
                issues_.push_back(
                    ValidationIssue{ValidationCode::ClipOverlap, overlap_path,
                                    "Clip overlaps predecessor in start order on track"});
            }
            if (track_valid_clips[i].end > max_end) {
                max_end = track_valid_clips[i].end;
            }
        }
    }

    void check_markers(const std::string& seq_path, const Sequence& seq) {
        std::unordered_set<MarkerId> seen_marker_ids;
        for (std::size_t m_idx = 0; m_idx < seq.markers.size(); ++m_idx) {
            if (!seen_marker_ids.insert(seq.markers[m_idx].id).second) {
                issues_.push_back(ValidationIssue{ValidationCode::DuplicateId,
                                                  seq_path + "/markers/" + std::to_string(m_idx),
                                                  "Duplicate MarkerId found within sequence"});
            }
        }
    }

    void check_compound_cycles() {
        const auto cycles = find_compound_cycles(project_);
        for (const auto& cycle : cycles) {
            if (cycle.empty()) {
                continue;
            }
            std::string cycle_str;
            for (const auto& id : cycle) {
                cycle_str += id.to_string();
                cycle_str += " -> ";
            }
            cycle_str += cycle[0].to_string();

            issues_.push_back(ValidationIssue{ValidationCode::CompoundCycle,
                                              "sequences/" + cycle[0].to_string(),
                                              "Compound clip cycle detected: " + cycle_str});
        }
    }

    void check_links() {
        for (const auto& [_, refs] : link_usage_) {
            if (refs.size() < 2) {
                issues_.push_back(ValidationIssue{ValidationCode::LinkIncomplete, refs[0].clip_path,
                                                  "LinkId must be shared by at least two clips"});
            } else {
                bool same_seq = true;
                for (std::size_t i = 1; i < refs.size(); ++i) {
                    if (refs[i].seq_key != refs[0].seq_key) {
                        same_seq = false;
                        break;
                    }
                }
                if (!same_seq) {
                    issues_.push_back(ValidationIssue{
                        ValidationCode::LinkAcrossSequences, refs[0].clip_path,
                        "All clips sharing a LinkId must reside in the same sequence"});
                }
            }
        }
    }

    const Project& project_;
    std::vector<ValidationIssue> issues_;
    std::unordered_set<TrackId> seen_track_ids_;
    std::unordered_set<ClipId> seen_clip_ids_;
    std::map<LinkId, std::vector<LinkRef>> link_usage_;
};

}  // namespace

std::string_view to_string(ValidationCode code) noexcept {
    switch (code) {
        case ValidationCode::IdMismatch:
            return "IdMismatch";
        case ValidationCode::DuplicateId:
            return "DuplicateId";
        case ValidationCode::MissingMainSequence:
            return "MissingMainSequence";
        case ValidationCode::InvalidCanvas:
            return "InvalidCanvas";
        case ValidationCode::InvalidDuration:
            return "InvalidDuration";
        case ValidationCode::NegativeStart:
            return "NegativeStart";
        case ValidationCode::ClipRangeOverflow:
            return "ClipRangeOverflow";
        case ValidationCode::ClipOverlap:
            return "ClipOverlap";
        case ValidationCode::ClipKindMismatch:
            return "ClipKindMismatch";
        case ValidationCode::MissingMedia:
            return "MissingMedia";
        case ValidationCode::MediaKindMismatch:
            return "MediaKindMismatch";
        case ValidationCode::SourceRangeOutOfBounds:
            return "SourceRangeOutOfBounds";
        case ValidationCode::LinkIncomplete:
            return "LinkIncomplete";
        case ValidationCode::LinkAcrossSequences:
            return "LinkAcrossSequences";
        case ValidationCode::MissingSequence:
            return "MissingSequence";
        case ValidationCode::CompoundCycle:
            return "CompoundCycle";
    }
    return "Unknown";
}

std::vector<ValidationIssue> collect_issues(const Project& project) {
    IssueCollector collector(project);
    return collector.collect();
}

core::Status validate(const Project& project) {
    const auto issues = collect_issues(project);
    if (issues.empty()) {
        return core::Status{};
    }

    const std::string msg = std::to_string(issues.size()) + " validation issue(s); first: " +
                            std::string(to_string(issues[0].code)) + " at " + issues[0].path +
                            ": " + issues[0].message;
    return core::make_error(core::ErrorCode::InvalidArgument, msg);
}

}  // namespace nxtcut::model
