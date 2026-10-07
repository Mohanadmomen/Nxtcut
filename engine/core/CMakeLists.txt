find_package(fmt REQUIRED)
find_package(tl-expected CONFIG REQUIRED)

nxtcut_add_module(
    NAME core
    SOURCES
        src/error.cpp
        src/frame_rate.cpp
        src/frame_time.cpp
        src/mul_div.cpp
        src/time.cpp
        src/timecode.cpp
        src/uuid.cpp
        src/version.cpp
    HEADERS
        include/nxtcut/core/error.hpp
        include/nxtcut/core/frame_rate.hpp
        include/nxtcut/core/frame_time.hpp
        include/nxtcut/core/id.hpp
        include/nxtcut/core/mul_div.hpp
        include/nxtcut/core/result.hpp
        include/nxtcut/core/time.hpp
        include/nxtcut/core/timecode.hpp
        include/nxtcut/core/uuid.hpp
        include/nxtcut/core/version.hpp
    DEPENDS
        tl::expected
    PRIVATE_DEPENDS
        fmt::fmt
)
