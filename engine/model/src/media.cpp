#include <nxtcut/model/media.hpp>

namespace nxtcut::model {

std::string_view to_string(MediaKind kind) noexcept {
    switch (kind) {
        case MediaKind::Video:
            return "Video";
        case MediaKind::Audio:
            return "Audio";
        case MediaKind::Image:
            return "Image";
    }
    return "Unknown";
}

}  // namespace nxtcut::model
