#pragma once

#include <nxtcut/commands/change_set.hpp>
#include <nxtcut/core/result.hpp>
#include <nxtcut/core/uuid.hpp>
#include <nxtcut/model/ids.hpp>
#include <nxtcut/model/media.hpp>
#include <nxtcut/model/project.hpp>

#include <string>

namespace nxtcut::commands {

/**
 * @brief Command to import and register a new media asset.
 */
struct AddMedia {
    model::MediaAsset asset;

    [[nodiscard]] std::string label() const { return "Add Media"; }
    [[nodiscard]] core::Result<ChangeSet> build(const model::Project& project,
                                                core::UuidGenerator& ids) const;
};

/**
 * @brief Command to remove an unused media asset from the project.
 */
struct RemoveMedia {
    model::MediaId media;

    [[nodiscard]] std::string label() const { return "Remove Media"; }
    [[nodiscard]] core::Result<ChangeSet> build(const model::Project& project,
                                                core::UuidGenerator& ids) const;
};

/**
 * @brief Command to update the filesystem path of an existing media asset.
 */
struct RelinkMedia {
    model::MediaId media;
    std::string new_path;

    [[nodiscard]] std::string label() const { return "Relink Media"; }
    [[nodiscard]] core::Result<ChangeSet> build(const model::Project& project,
                                                core::UuidGenerator& ids) const;
};

}  // namespace nxtcut::commands
