#include <nxtcut/model/project.hpp>

namespace nxtcut::model {

const MediaAsset* find_media(const Project& project, MediaId id) noexcept {
    const auto it = project.media.find(id);
    if (it != project.media.end()) {
        return &it->second;
    }
    return nullptr;
}

const Sequence* find_sequence(const Project& project, SequenceId id) noexcept {
    const auto it = project.sequences.find(id);
    if (it != project.sequences.end()) {
        return &it->second;
    }
    return nullptr;
}

}  // namespace nxtcut::model
