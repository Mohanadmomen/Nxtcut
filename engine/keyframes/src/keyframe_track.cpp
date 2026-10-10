#include <nxtcut/keyframes/keyframe_track.hpp>

namespace nxtcut::keyframes {

template class KeyframeTrack<double>;
template class KeyframeTrack<core::Color>;
template class KeyframeTrack<core::Point<double>>;

}  // namespace nxtcut::keyframes
