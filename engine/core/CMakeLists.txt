find_package(fmt REQUIRED)
find_package(tl-expected CONFIG REQUIRED)
find_package(Threads REQUIRED)

nxtcut_add_module(
    NAME core
    SOURCES
        src/affine_transform.cpp
        src/cancellation.cpp
        src/color.cpp
        src/console_sink.cpp
        src/error.cpp
        src/file_sink.cpp
        src/frame_rate.cpp
        src/frame_time.cpp
        src/log_format.cpp
        src/log_level.cpp
        src/logger.cpp
        src/memory_sink.cpp
        src/mul_div.cpp
        src/thread_pool.cpp
        src/time.cpp
        src/timecode.cpp
        src/uuid.cpp
        src/version.cpp
    HEADERS
        include/nxtcut/core/affine_transform.hpp
        include/nxtcut/core/cancellation.hpp
        include/nxtcut/core/color.hpp
        include/nxtcut/core/console_sink.hpp
        include/nxtcut/core/error.hpp
        include/nxtcut/core/file_sink.hpp
        include/nxtcut/core/frame_rate.hpp
        include/nxtcut/core/frame_time.hpp
        include/nxtcut/core/geometry.hpp
        include/nxtcut/core/id.hpp
        include/nxtcut/core/log_format.hpp
        include/nxtcut/core/log_level.hpp
        include/nxtcut/core/log_sink.hpp
        include/nxtcut/core/logger.hpp
        include/nxtcut/core/memory_sink.hpp
        include/nxtcut/core/mul_div.hpp
        include/nxtcut/core/mul_div_detail.hpp
        include/nxtcut/core/result.hpp
        include/nxtcut/core/thread_pool.hpp
        include/nxtcut/core/time.hpp
        include/nxtcut/core/timecode.hpp
        include/nxtcut/core/unique_task.hpp
        include/nxtcut/core/uuid.hpp
        include/nxtcut/core/version.hpp
    DEPENDS
        tl::expected
        Threads::Threads
    PRIVATE_DEPENDS
        fmt::fmt
)

