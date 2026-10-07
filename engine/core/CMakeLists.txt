find_package(fmt REQUIRED)

freecut_add_module(
    NAME core
    SOURCES
        src/version.cpp
    HEADERS
        include/freecut/core/version.hpp
    PRIVATE_DEPENDS
        fmt::fmt
)
