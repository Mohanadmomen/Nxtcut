nxtcut_add_module_test(
    NAME commands
    SOURCES
        apply_test.cpp
        change_set_test.cpp
        clip_commands_test.cpp
        editor_test.cpp
        history_test.cpp
        marker_commands_test.cpp
        media_commands_test.cpp
        project_commands_test.cpp
        sequence_commands_test.cpp
        track_commands_test.cpp
    DEPENDS
        nxtcut_test_support
)
