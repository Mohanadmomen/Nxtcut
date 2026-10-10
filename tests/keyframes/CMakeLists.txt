nxtcut_add_module_test(
    NAME keyframes
    SOURCES
        animatable_test.cpp
        easing_test.cpp
        interpolation_test.cpp
        keyframe_track_cursor_test.cpp
        keyframe_track_test.cpp
        keyframe_track_types_test.cpp
    DEPENDS
        nxtcut_test_support
)
