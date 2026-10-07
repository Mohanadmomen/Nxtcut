find_package(fmt REQUIRED)

nxtcut_add_module(
    NAME core
    SOURCES
        src/version.cpp
    HEADERS
        include/nxtcut/core/version.hpp
    PRIVATE_DEPENDS
        fmt::fmt
)
