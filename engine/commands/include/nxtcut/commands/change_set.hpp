#pragma once

#include <nxtcut/core/color.hpp>
#include <nxtcut/core/frame_rate.hpp>
#include <nxtcut/core/geometry.hpp>
#include <nxtcut/model/clip.hpp>
#include <nxtcut/model/ids.hpp>
#include <nxtcut/model/media.hpp>
#include <nxtcut/model/sequence.hpp>
#include <nxtcut/model/track.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace nxtcut::commands {

/**
 * @brief Plain properties descriptor for Project-level identity.
 */
struct ProjectProperties {
    std::string name;
    model::SequenceId main_sequence;
};

/**
 * @brief Plain configuration descriptor for Sequence settings.
 */
struct SequenceSettings {
    std::string name;
    core::FrameRate frame_rate{core::frame_rates::k30};
    core::Size<std::int32_t> canvas{1920, 1080};
    core::SampleRate sample_rate{core::sample_rates::k48000};
    core::Color background{core::Color{0.0f, 0.0f, 0.0f, 1.0f}};
};

/**
 * @brief Plain properties descriptor for Track attributes.
 */
struct TrackProperties {
    std::string name;
    bool enabled{true};
    bool locked{false};
};

/**
 * @brief Positioned track slot within a sequence's tracks list.
 */
struct TrackSlot {
    std::size_t index{0};
    model::Track track;
};

/**
 * @brief Project-level property mutation record.
 */
struct ProjectPropertiesChange {
    ProjectProperties before;
    ProjectProperties after;
};

/**
 * @brief Media asset lifecycle or property mutation record.
 */
struct MediaChange {
    model::MediaId id;
    std::optional<model::MediaAsset> before;
    std::optional<model::MediaAsset> after;
};

/**
 * @brief Sequence lifecycle record (creation or deletion). Exactly one side present.
 */
struct SequenceChange {
    model::SequenceId id;
    std::optional<model::Sequence> before;
    std::optional<model::Sequence> after;
};

/**
 * @brief Sequence settings mutation record.
 */
struct SequenceSettingsChange {
    model::SequenceId id;
    SequenceSettings before;
    SequenceSettings after;
};

/**
 * @brief Track lifecycle record (creation or deletion). Exactly one side present.
 */
struct TrackChange {
    model::SequenceId sequence;
    model::TrackId id;
    std::optional<TrackSlot> before;
    std::optional<TrackSlot> after;
};

/**
 * @brief Track reordering record within a sequence.
 */
struct TrackMove {
    model::SequenceId sequence;
    model::TrackId id;
    std::size_t from_index{0};
    std::size_t to_index{0};
};

/**
 * @brief Track properties mutation record.
 */
struct TrackPropertiesChange {
    model::SequenceId sequence;
    model::TrackId id;
    TrackProperties before;
    TrackProperties after;
};

/**
 * @brief Clip lifecycle or property mutation record.
 * Add: before empty; remove: after empty; modify: both present.
 */
struct ClipChange {
    model::SequenceId sequence;
    model::TrackId track;
    model::ClipId id;
    std::optional<model::Clip> before;
    std::optional<model::Clip> after;
};

/**
 * @brief Marker lifecycle or property mutation record.
 */
struct MarkerChange {
    model::SequenceId sequence;
    model::MarkerId id;
    std::optional<model::Marker> before;
    std::optional<model::Marker> after;
};

/**
 * @brief Variant over all granular document change records.
 */
using Change =
    std::variant<ProjectPropertiesChange, MediaChange, SequenceChange, SequenceSettingsChange,
                 TrackChange, TrackMove, TrackPropertiesChange, ClipChange, MarkerChange>;

/**
 * @brief An ordered collection of fine-grained document mutations representing one atomic edit.
 */
struct ChangeSet {
    std::string label;
    std::vector<Change> changes;

    [[nodiscard]] bool empty() const noexcept { return changes.empty(); }
};

/**
 * @brief Computes the inverse ChangeSet with entries in reverse order and sides swapped.
 */
[[nodiscard]] ChangeSet inverse(const ChangeSet& change_set);

/**
 * @brief Summary receipt listing all entity IDs created by a ChangeSet in entry order.
 */
struct EditReceipt {
    std::vector<model::SequenceId> created_sequences;
    std::vector<model::TrackId> created_tracks;
    std::vector<model::ClipId> created_clips;
    std::vector<model::MarkerId> created_markers;
    std::vector<model::MediaId> created_media;

    [[nodiscard]] bool operator==(const EditReceipt&) const noexcept = default;
};

/**
 * @brief Evaluates an EditReceipt for the provided ChangeSet.
 */
[[nodiscard]] EditReceipt receipt_of(const ChangeSet& change_set);

}  // namespace nxtcut::commands
