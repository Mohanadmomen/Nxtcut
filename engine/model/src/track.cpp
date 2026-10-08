#include <nxtcut/model/track.hpp>

namespace nxtcut::model {

std::string_view to_string(TrackKind kind) noexcept {
    switch (kind) {
        case TrackKind::Video:
            return "Video";
        case TrackKind::Audio:
            return "Audio";
    }
    return "Unknown";
}

}  // namespace nxtcut::model
