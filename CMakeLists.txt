cmake_minimum_required(VERSION 3.25)

project(freecut
    VERSION 0.1.0
    DESCRIPTION "FreeCut-CPP multi-track desktop video editor"
    HOMEPAGE_URL "https://github.com/freecut/freecut-cpp"
    LANGUAGES CXX
)

# Standard C++20 configuration
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for language servers and clang-tidy
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build options
option(FREECUT_BUILD_TESTS "Build FreeCut test suites" ON)

# Include custom CMake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)
include(Sanitizers)
include(FreecutModule)

# UI-free engine library modules
add_subdirectory(engine)

# Tests and architectural validation
if(FREECUT_BUILD_TESTS)
    enable_testing()

    # Register architectural boundary checker as a CTest test
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_test(
        NAME check_architecture
        COMMAND Python3::Interpreter
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/check_architecture.py"
            "--root" "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    add_subdirectory(tests)
endif()
