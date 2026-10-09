#include <nxtcut/commands/project_commands.hpp>

namespace nxtcut::commands {

core::Result<ChangeSet> SetProjectProperties::build(const model::Project& project,
                                                    core::UuidGenerator& ids) const {
    static_cast<void>(ids);
    if (!name.has_value() && !main_sequence.has_value()) {
        return core::make_error(core::ErrorCode::InvalidArgument,
                                "at least one project property must be specified");
    }

    if (main_sequence.has_value()) {
        if (!project.sequences.contains(*main_sequence)) {
            return core::make_error(core::ErrorCode::NotFound, "main sequence not found");
        }
    }

    ProjectProperties before{project.name, project.main_sequence};
    ProjectProperties after{name.value_or(before.name),
                            main_sequence.value_or(before.main_sequence)};

    if (after.name == before.name && after.main_sequence == before.main_sequence) {
        return ChangeSet{label(), {}};
    }

    ChangeSet cs;
    cs.label = label();
    cs.changes.push_back(ProjectPropertiesChange{before, after});
    return cs;
}

}  // namespace nxtcut::commands
