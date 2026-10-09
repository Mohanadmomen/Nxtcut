#pragma once

#include <nxtcut/commands/change_set.hpp>
#include <nxtcut/commands/timeline_edit_types.hpp>
#include <nxtcut/core/result.hpp>
#include <nxtcut/core/time.hpp>
#include <nxtcut/core/uuid.hpp>
#include <nxtcut/model/clip.hpp>
#include <nxtcut/model/ids.hpp>
#include <nxtcut/model/project.hpp>
#include <nxtcut/model/sequence.hpp>
#include <nxtcut/model/time_coords.hpp>
#include <nxtcut/model/track.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace nxtcut::commands::detail {

/**
 * @brief Performs checked addition of timeline time and duration.
 */
[[nodiscard]] core::Result<model::TimelineTime> add_time(model::TimelineTime t,
                                                         core::Duration d) noexcept;

/**
 * @brief Performs checked subtraction of duration from timeline time.
 */
[[nodiscard]] core::Result<model::TimelineTime> sub_time(model::TimelineTime t,
                                                         core::Duration d) noexcept;

/**
 * @brief Performs checked subtraction of two timeline times to compute duration.
 */
[[nodiscard]] core::Result<core::Duration> diff_time(model::TimelineTime a,
                                                     model::TimelineTime b) noexcept;

/**
 * @brief Ensures audio content fades satisfy the duration bounds.
 */
void fit_audio_fades(model::Clip& clip) noexcept;

/**
 * @brief Working copy of a single track's clips during command execution.
 */
struct ScratchTrack {
    std::size_t index{0};
    model::TrackId id;
    model::TrackKind kind{model::TrackKind::Video};
    bool locked{false};
    std::vector<model::Clip> original_clips;
    std::vector<model::Clip> clips;
    bool touched{false};
};

/**
 * @brief Working sandbox environment for sequence timeline edits.
 */
class ScratchTimeline {
public:
    [[nodiscard]] static core::Result<ScratchTimeline> create(const model::Project& project,
                                                              model::SequenceId seq_id,
                                                              core::UuidGenerator& ids);

    [[nodiscard]] const model::Project& project() const noexcept { return project_; }
    [[nodiscard]] const model::Sequence& sequence() const noexcept { return *sequence_; }
    [[nodiscard]] model::SequenceId sequence_id() const noexcept { return sequence_id_; }
    [[nodiscard]] core::UuidGenerator& ids() noexcept { return ids_; }
    [[nodiscard]] std::vector<ScratchTrack>& tracks() noexcept { return tracks_; }
    [[nodiscard]] const std::vector<ScratchTrack>& tracks() const noexcept { return tracks_; }
    [[nodiscard]] std::unordered_set<model::LinkId>& touched_links() noexcept {
        return touched_links_;
    }

    [[nodiscard]] ScratchTrack* find_track(model::TrackId track_id) noexcept;
    [[nodiscard]] const ScratchTrack* find_track(model::TrackId track_id) const noexcept;

    [[nodiscard]] std::pair<ScratchTrack*, model::Clip*> find_clip(model::ClipId clip_id) noexcept;
    [[nodiscard]] std::pair<const ScratchTrack*, const model::Clip*> find_clip(
        model::ClipId clip_id) const noexcept;

    [[nodiscard]] core::Result<model::TimelineTime> snap_time(model::TimelineTime t) const noexcept;
    [[nodiscard]] core::Result<core::Duration> snap_duration(core::Duration d) const noexcept;

    [[nodiscard]] bool is_clip_compatible(model::TrackKind track_kind,
                                          const model::ClipContent& content) const noexcept;

    [[nodiscard]] core::Status validate_clip_media_and_source(const model::Clip& clip) const;

    [[nodiscard]] std::vector<model::ClipId> expand_links(
        const std::vector<model::ClipId>& clip_ids) const;

    [[nodiscard]] core::Result<std::pair<model::Clip, model::Clip>> split_clip(
        const model::Clip& clip, model::TimelineTime at);

    [[nodiscard]] core::Status trim_head(model::Clip& clip, model::TimelineTime new_start);
    [[nodiscard]] core::Status trim_tail(model::Clip& clip, model::TimelineTime new_end);

    [[nodiscard]] core::Status check_overlaps_on_touched_tracks() const;

    [[nodiscard]] core::Status repair_links();

    [[nodiscard]] core::Result<ChangeSet> diff_to_changes(const std::string& label) const;

private:
    ScratchTimeline(const model::Project& project, model::SequenceId seq_id,
                    const model::Sequence& sequence, core::UuidGenerator& ids)
        : project_(project), sequence_id_(seq_id), sequence_(&sequence), ids_(ids) {}

    const model::Project& project_;
    model::SequenceId sequence_id_;
    const model::Sequence* sequence_{nullptr};
    core::UuidGenerator& ids_;
    std::vector<ScratchTrack> tracks_;
    std::unordered_set<model::LinkId> touched_links_;
};

}  // namespace nxtcut::commands::detail
