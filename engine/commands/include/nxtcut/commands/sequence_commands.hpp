#pragma once

#include <nxtcut/commands/change_set.hpp>
#include <nxtcut/core/color.hpp>
#include <nxtcut/core/frame_rate.hpp>
#include <nxtcut/core/geometry.hpp>
#include <nxtcut/core/result.hpp>
#include <nxtcut/core/uuid.hpp>
#include <nxtcut/model/ids.hpp>
#include <nxtcut/model/project.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace nxtcut::commands {

/**
 * @brief Command to create a new empty sequence in the project.
 */
struct CreateSequence {
    std::string name;
    core::FrameRate frame_rate{core::frame_rates::k30};
    core::Size<std::int32_t> canvas{1920, 1080};
    core::SampleRate sample_rate{core::sample_rates::k48000};
    core::Color background{core::Color{0.0f, 0.0f, 0.0f, 1.0f}};

    [[nodiscard]] std::string label() const { return "Create Sequence"; }
    [[nodiscard]] core::Result<ChangeSet> build(const model::Project& project,
                                                core::UuidGenerator& ids) const;
};

/**
 * @brief Command to delete an unused sequence from the project.
 */
struct RemoveSequence {
    model::SequenceId sequence;

    [[nodiscard]] std::string label() const { return "Remove Sequence"; }
    [[nodiscard]] core::Result<ChangeSet> build(const model::Project& project,
                                                core::UuidGenerator& ids) const;
};

/**
 * @brief Command to modify settings of an existing sequence.
 */
struct SetSequenceSettings {
    model::SequenceId sequence;
    std::optional<std::string> name;
    std::optional<core::FrameRate> frame_rate;
    std::optional<core::Size<std::int32_t>> canvas;
    std::optional<core::SampleRate> sample_rate;
    std::optional<core::Color> background;

    [[nodiscard]] std::string label() const { return "Set Sequence Settings"; }
    [[nodiscard]] core::Result<ChangeSet> build(const model::Project& project,
                                                core::UuidGenerator& ids) const;
};

}  // namespace nxtcut::commands
