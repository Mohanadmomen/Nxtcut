#pragma once

#include <nxtcut/commands/change_set.hpp>
#include <nxtcut/core/result.hpp>
#include <nxtcut/core/time.hpp>
#include <nxtcut/core/uuid.hpp>
#include <nxtcut/model/blend_mode.hpp>
#include <nxtcut/model/clip.hpp>
#include <nxtcut/model/ids.hpp>
#include <nxtcut/model/project.hpp>
#include <nxtcut/model/transform.hpp>

#include <optional>
#include <string>

namespace nxtcut::commands {

/**
 * @brief Command to update generic visual clip properties.
 */
struct SetClipProperties {
    model::SequenceId sequence;
    model::ClipId clip;
    std::optional<std::string> name;
    std::optional<bool> enabled;
    std::optional<model::BlendMode> blend_mode;
    std::optional<model::TransformProps> transform;

    [[nodiscard]] std::string label() const { return "Set Clip Properties"; }
    [[nodiscard]] core::Result<ChangeSet> build(const model::Project& project,
                                                core::UuidGenerator& ids) const;
};

/**
 * @brief Command to update audio clip gain and fade parameters.
 */
struct SetAudioClipProperties {
    model::SequenceId sequence;
    model::ClipId clip;
    std::optional<double> volume;
    std::optional<core::Duration> fade_in;
    std::optional<core::Duration> fade_out;

    [[nodiscard]] std::string label() const { return "Set Audio Clip Properties"; }
    [[nodiscard]] core::Result<ChangeSet> build(const model::Project& project,
                                                core::UuidGenerator& ids) const;
};

}  // namespace nxtcut::commands
