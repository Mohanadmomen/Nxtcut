nxtcut_add_module(
    NAME keyframes
    SOURCES
        src/easing.cpp
        src/interpolation.cpp
        src/keyframe_track.cpp
    HEADERS
        include/nxtcut/keyframes/animatable.hpp
        include/nxtcut/keyframes/easing.hpp
        include/nxtcut/keyframes/interpolation.hpp
        include/nxtcut/keyframes/keyframe.hpp
        include/nxtcut/keyframes/keyframe_track.hpp
    DEPENDS
        nxtcut::core
)
