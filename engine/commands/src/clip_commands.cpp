#include <nxtcut/commands/clip_commands.hpp>
#include <nxtcut/model/checked_arithmetic.hpp>
#include <nxtcut/model/equality.hpp>

#include <bit>
#include <cstdint>
#include <string>
#include <variant>

namespace nxtcut::commands {

core::Result<ChangeSet> SetClipProperties::build(const model::Project& project,
                                                 core::UuidGenerator& ids) const {
    static_cast<void>(ids);
    if (!name.has_value() && !enabled.has_value() && !blend_mode.has_value() &&
        !transform.has_value()) {
        return core::make_error(core::ErrorCode::InvalidArgument,
                                "at least one clip property must be specified");
    }

    auto seq_it = project.sequences.find(sequence);
    if (seq_it == project.sequences.end()) {
        return core::make_error(core::ErrorCode::NotFound, "sequence not found");
    }

    const model::Track* containing_track = nullptr;
    const model::Clip* clip_ptr = nullptr;
    for (const auto& track : seq_it->second.tracks) {
        for (const auto& c : track.clips) {
            if (c.id == clip) {
                containing_track = &track;
                clip_ptr = &c;
                break;
            }
        }
        if (clip_ptr != nullptr) {
            break;
        }
    }

    if (clip_ptr == nullptr || containing_track == nullptr) {
        return core::make_error(core::ErrorCode::NotFound, "clip not found");
    }

    if (containing_track->locked) {
        return core::make_error(core::ErrorCode::InvalidArgument,
                                "cannot modify clip on a locked track");
    }

    if (transform.has_value()) {
        const double op = transform->opacity.constant_value();
        if (op < 0.0 || op > 1.0) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "transform opacity must be in range [0, 1]");
        }
        const double cl = transform->crop_left.constant_value();
        const double cr = transform->crop_right.constant_value();
        const double ct = transform->crop_top.constant_value();
        const double cb = transform->crop_bottom.constant_value();
        if (cl < 0.0 || cl > 1.0 || cr < 0.0 || cr > 1.0 || ct < 0.0 || ct > 1.0 || cb < 0.0 ||
            cb > 1.0) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "crop fractions must be in range [0, 1]");
        }
        if (cl + cr > 1.0) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "crop_left + crop_right must be <= 1.0");
        }
        if (ct + cb > 1.0) {
            return core::make_error(core::ErrorCode::InvalidArgument,
                                    "crop_top + crop_bottom must be <= 1.0");
        }
    }

    const bool name_unchanged = !name.has_value() || (*name == clip_ptr->name);
    const bool enabled_unchanged = !enabled.has_value() || (*enabled == clip_ptr->enabled);
    const bool blend_unchanged = !blend_mode.has_value() || (*blend_mode == clip_ptr->blend_mode);
    const bool transform_unchanged =
        !transform.has_value() || model::identical(*transform, clip_ptr->transform);

    if (name_unchanged && enabled_unchanged && blend_unchanged && transform_unchanged) {
        return ChangeSet{label(), {}};
    }

    model::Clip after_clip = *clip_ptr;
    if (name.has_value()) {
        after_clip.name = *name;
    }
    if (enabled.has_value()) {
        after_clip.enabled = *enabled;
    }
    if (blend_mode.has_value()) {
        after_clip.blend_mode = *blend_mode;
    }
    if (transform.has_value()) {
        after_clip.transform = *transform;
    }

    ChangeSet cs;
    cs.label = label();
    cs.changes.push_back(
        ClipChange{sequence, containing_track->id, clip, *clip_ptr, std::move(after_clip)});
    return cs;
}

core::Result<ChangeSet> SetAudioClipProperties::build(const model::Project& project,
                                                      core::UuidGenerator& ids) const {
    static_cast<void>(ids);
    if (!volume.has_value() && !fade_in.has_value() && !fade_out.has_value()) {
        return core::make_error(core::ErrorCode::InvalidArgument,
                                "at least one audio property must be specified");
    }

    auto seq_it = project.sequences.find(sequence);
    if (seq_it == project.sequences.end()) {
        return core::make_error(core::ErrorCode::NotFound, "sequence not found");
    }

    const model::Track* containing_track = nullptr;
    const model::Clip* clip_ptr = nullptr;
    for (const auto& track : seq_it->second.tracks) {
        for (const auto& c : track.clips) {
            if (c.id == clip) {
                containing_track = &track;
                clip_ptr = &c;
                break;
            }
        }
        if (clip_ptr != nullptr) {
            break;
        }
    }

    if (clip_ptr == nullptr || containing_track == nullptr) {
        return core::make_error(core::ErrorCode::NotFound, "clip not found");
    }

    if (containing_track->locked) {
        return core::make_error(core::ErrorCode::InvalidArgument,
                                "cannot modify clip on a locked track");
    }

    if (!std::holds_alternative<model::AudioContent>(clip_ptr->content)) {
        return core::make_error(core::ErrorCode::InvalidArgument,
                                "clip does not contain audio content");
    }

    const auto& current_audio = std::get<model::AudioContent>(clip_ptr->content);

    if (volume.has_value() && *volume < 0.0) {
        return core::make_error(core::ErrorCode::InvalidArgument, "volume must be non-negative");
    }

    const core::Duration fin = fade_in.value_or(current_audio.fade_in);
    const core::Duration fout = fade_out.value_or(current_audio.fade_out);
    if (fin.ticks() < 0 || fout.ticks() < 0) {
        return core::make_error(core::ErrorCode::InvalidArgument,
                                "fade durations must be non-negative");
    }

    const auto sum_ticks = model::detail::checked_add(fin.ticks(), fout.ticks());
    if (!sum_ticks.has_value() || *sum_ticks > clip_ptr->duration.ticks()) {
        return core::make_error(core::ErrorCode::InvalidArgument,
                                "fade durations sum exceeds clip duration");
    }

    const double current_vol = current_audio.volume.constant_value();
    const bool vol_unchanged = !volume.has_value() || (std::bit_cast<std::uint64_t>(*volume) ==
                                                       std::bit_cast<std::uint64_t>(current_vol));
    const bool fin_unchanged =
        !fade_in.has_value() || (fade_in->ticks() == current_audio.fade_in.ticks());
    const bool fout_unchanged =
        !fade_out.has_value() || (fade_out->ticks() == current_audio.fade_out.ticks());

    if (vol_unchanged && fin_unchanged && fout_unchanged) {
        return ChangeSet{label(), {}};
    }

    model::Clip after_clip = *clip_ptr;
    model::AudioContent updated_audio = current_audio;
    if (volume.has_value()) {
        updated_audio.volume.set_constant(*volume);
    }
    if (fade_in.has_value()) {
        updated_audio.fade_in = *fade_in;
    }
    if (fade_out.has_value()) {
        updated_audio.fade_out = *fade_out;
    }
    after_clip.content = std::move(updated_audio);

    ChangeSet cs;
    cs.label = label();
    cs.changes.push_back(
        ClipChange{sequence, containing_track->id, clip, *clip_ptr, std::move(after_clip)});
    return cs;
}

}  // namespace nxtcut::commands
