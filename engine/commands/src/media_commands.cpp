#include <nxtcut/commands/media_commands.hpp>
#include <nxtcut/model/clip.hpp>

#include <string>
#include <type_traits>
#include <variant>

namespace nxtcut::commands {

core::Result<ChangeSet> AddMedia::build(const model::Project& project,
                                        core::UuidGenerator& ids) const {
    if (asset.path.empty()) {
        return core::make_error(core::ErrorCode::InvalidArgument, "media path cannot be empty");
    }

    model::MediaAsset target_asset = asset;
    if (target_asset.id.is_nil()) {
        target_asset.id = model::generate_id<model::MediaId>(ids);
    } else {
        if (project.media.contains(target_asset.id)) {
            return core::make_error(core::ErrorCode::AlreadyExists, "media id already exists");
        }
    }

    if (target_asset.kind == model::MediaKind::Video) {
        if (!target_asset.video.has_value() || target_asset.duration <= core::Duration::zero()) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "video media requires video stream info and positive duration");
        }
    } else if (target_asset.kind == model::MediaKind::Audio) {
        if (!target_asset.audio.has_value() || target_asset.duration <= core::Duration::zero()) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "audio media requires audio stream info and positive duration");
        }
    }

    ChangeSet cs;
    cs.label = label();
    cs.changes.push_back(MediaChange{target_asset.id, std::nullopt, target_asset});
    return cs;
}

core::Result<ChangeSet> RemoveMedia::build(const model::Project& project,
                                           core::UuidGenerator& ids) const {
    static_cast<void>(ids);
    auto it = project.media.find(media);
    if (it == project.media.end()) {
        return core::make_error(core::ErrorCode::NotFound, "media asset not found");
    }

    bool in_use = false;
    for (const auto& [seq_id, seq] : project.sequences) {
        static_cast<void>(seq_id);
        for (const auto& track : seq.tracks) {
            for (const auto& clip : track.clips) {
                std::visit(
                    [&in_use, this](const auto& content) {
                        using T = std::decay_t<decltype(content)>;
                        if constexpr (std::is_same_v<T, model::VideoContent> ||
                                      std::is_same_v<T, model::AudioContent> ||
                                      std::is_same_v<T, model::ImageContent>) {
                            if (content.media == media) {
                                in_use = true;
                            }
                        }
                    },
                    clip.content);
                if (in_use) {
                    break;
                }
            }
            if (in_use) {
                break;
            }
        }
        if (in_use) {
            break;
        }
    }

    if (in_use) {
        return core::make_error(core::ErrorCode::InvalidArgument,
                                "cannot remove media asset: in use by timeline clips");
    }

    ChangeSet cs;
    cs.label = label();
    cs.changes.push_back(MediaChange{media, it->second, std::nullopt});
    return cs;
}

core::Result<ChangeSet> RelinkMedia::build(const model::Project& project,
                                           core::UuidGenerator& ids) const {
    static_cast<void>(ids);
    if (new_path.empty()) {
        return core::make_error(core::ErrorCode::InvalidArgument, "relink path cannot be empty");
    }

    auto it = project.media.find(media);
    if (it == project.media.end()) {
        return core::make_error(core::ErrorCode::NotFound, "media asset not found");
    }

    if (it->second.path == new_path) {
        return ChangeSet{label(), {}};
    }

    model::MediaAsset updated = it->second;
    updated.path = new_path;

    ChangeSet cs;
    cs.label = label();
    cs.changes.push_back(MediaChange{media, it->second, updated});
    return cs;
}

}  // namespace nxtcut::commands
