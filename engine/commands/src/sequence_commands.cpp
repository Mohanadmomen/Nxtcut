#include <nxtcut/commands/sequence_commands.hpp>
#include <nxtcut/model/clip.hpp>
#include <nxtcut/model/equality.hpp>

#include <string>
#include <utility>
#include <variant>

namespace nxtcut::commands {

core::Result<ChangeSet> CreateSequence::build(const model::Project& project,
                                              core::UuidGenerator& ids) const {
    static_cast<void>(project);
    if (canvas.width <= 0 || canvas.height <= 0) {
        return core::make_error(core::ErrorCode::InvalidArgument,
                                "canvas dimensions must be strictly positive");
    }

    const model::SequenceId seq_id = model::generate_id<model::SequenceId>(ids);
    model::Sequence seq;
    seq.id = seq_id;
    seq.name = name;
    seq.frame_rate = frame_rate;
    seq.canvas = canvas;
    seq.sample_rate = sample_rate;
    seq.background = background;

    ChangeSet cs;
    cs.label = label();
    cs.changes.push_back(SequenceChange{seq_id, std::nullopt, std::move(seq)});
    return cs;
}

core::Result<ChangeSet> RemoveSequence::build(const model::Project& project,
                                              core::UuidGenerator& ids) const {
    static_cast<void>(ids);
    auto it = project.sequences.find(sequence);
    if (it == project.sequences.end()) {
        return core::make_error(core::ErrorCode::NotFound, "sequence not found");
    }

    if (sequence == project.main_sequence) {
        return core::make_error(core::ErrorCode::InvalidArgument,
                                "cannot remove the main sequence");
    }

    bool in_use = false;
    for (const auto& [seq_id, seq] : project.sequences) {
        static_cast<void>(seq_id);
        for (const auto& track : seq.tracks) {
            for (const auto& clip : track.clips) {
                if (std::holds_alternative<model::CompoundContent>(clip.content)) {
                    if (std::get<model::CompoundContent>(clip.content).sequence == sequence) {
                        in_use = true;
                        break;
                    }
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
                                "cannot remove sequence: in use by compound clips");
    }

    ChangeSet cs;
    cs.label = label();
    cs.changes.push_back(SequenceChange{sequence, it->second, std::nullopt});
    return cs;
}

core::Result<ChangeSet> SetSequenceSettings::build(const model::Project& project,
                                                   core::UuidGenerator& ids) const {
    static_cast<void>(ids);
    if (!name.has_value() && !frame_rate.has_value() && !canvas.has_value() &&
        !sample_rate.has_value() && !background.has_value()) {
        return core::make_error(core::ErrorCode::InvalidArgument,
                                "at least one sequence setting must be specified");
    }

    if (canvas.has_value() && (canvas->width <= 0 || canvas->height <= 0)) {
        return core::make_error(core::ErrorCode::InvalidArgument,
                                "canvas dimensions must be strictly positive");
    }

    auto it = project.sequences.find(sequence);
    if (it == project.sequences.end()) {
        return core::make_error(core::ErrorCode::NotFound, "sequence not found");
    }

    const auto& current = it->second;
    SequenceSettings before{current.name, current.frame_rate, current.canvas, current.sample_rate,
                            current.background};
    SequenceSettings after{name.value_or(before.name), frame_rate.value_or(before.frame_rate),
                           canvas.value_or(before.canvas), sample_rate.value_or(before.sample_rate),
                           background.value_or(before.background)};

    const bool name_unchanged = !name.has_value() || (*name == before.name);
    const bool fr_unchanged = !frame_rate.has_value() || (*frame_rate == before.frame_rate);
    const bool canvas_unchanged = !canvas.has_value() || (*canvas == before.canvas);
    const bool sr_unchanged = !sample_rate.has_value() || (*sample_rate == before.sample_rate);
    const bool bg_unchanged =
        !background.has_value() || model::identical(*background, before.background);

    if (name_unchanged && fr_unchanged && canvas_unchanged && sr_unchanged && bg_unchanged) {
        return ChangeSet{label(), {}};
    }

    ChangeSet cs;
    cs.label = label();
    cs.changes.push_back(SequenceSettingsChange{sequence, before, after});
    return cs;
}

}  // namespace nxtcut::commands
