# cmake/FreecutModule.cmake
# Modern CMake helper functions to declare FreeCut engine modules and tests.

include(CMakeParseArguments)
include(CompilerWarnings)
include(Sanitizers)

# freecut_add_module(NAME <module_name> [SOURCES ...] [HEADERS ...] [DEPENDS ...] [PRIVATE_DEPENDS ...])
# Declares a static library `freecut_<name>` with alias `freecut::<name>`.
# Public headers live in `include/freecut/<name>/`, private code in `src/`.
# Public include directory is set to `include/`.
function(freecut_add_module)
    cmake_parse_arguments(
        MODULE
        ""
        "NAME"
        "SOURCES;HEADERS;DEPENDS;PRIVATE_DEPENDS"
        ${ARGN}
    )

    if(NOT MODULE_NAME)
        message(FATAL_ERROR "freecut_add_module requires a NAME argument.")
    endif()

    set(target_name "freecut_${MODULE_NAME}")

    # Discover sources and headers if not explicitly specified
    if(NOT MODULE_SOURCES)
        file(GLOB_RECURSE MODULE_SOURCES
            LIST_DIRECTORIES false
            "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
        )
    endif()

    if(NOT MODULE_HEADERS)
        file(GLOB_RECURSE MODULE_HEADERS
            LIST_DIRECTORIES false
            "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
        )
    endif()

    add_library(${target_name} STATIC
        ${MODULE_SOURCES}
        ${MODULE_HEADERS}
    )

    add_library(freecut::${MODULE_NAME} ALIAS ${target_name})

    target_compile_features(${target_name} PUBLIC cxx_std_20)

    target_include_directories(${target_name}
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    freecut_apply_warnings(${target_name})
    freecut_apply_sanitizers(${target_name})

    if(MODULE_DEPENDS)
        target_link_libraries(${target_name} PUBLIC ${MODULE_DEPENDS})
    endif()

    if(MODULE_PRIVATE_DEPENDS)
        target_link_libraries(${target_name} PRIVATE ${MODULE_PRIVATE_DEPENDS})
    endif()
endfunction()

# freecut_add_module_test(NAME <module_name> [SOURCES ...] [DEPENDS ...] [PRIVATE_DEPENDS ...])
# Declares a test executable `freecut_test_<name>`, links `freecut::<name>` and GTest,
# and registers discovered tests with CTest.
function(freecut_add_module_test)
    cmake_parse_arguments(
        TEST
        ""
        "NAME"
        "SOURCES;DEPENDS;PRIVATE_DEPENDS"
        ${ARGN}
    )

    if(NOT TEST_NAME)
        message(FATAL_ERROR "freecut_add_module_test requires a NAME argument.")
    endif()

    set(target_name "freecut_test_${TEST_NAME}")

    if(NOT TEST_SOURCES)
        file(GLOB_RECURSE TEST_SOURCES
            LIST_DIRECTORIES false
            "${CMAKE_CURRENT_SOURCE_DIR}/*_test.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/*_tests.cpp"
        )
    endif()

    add_executable(${target_name}
        ${TEST_SOURCES}
    )

    target_compile_features(${target_name} PRIVATE cxx_std_20)

    freecut_apply_warnings(${target_name})
    freecut_apply_sanitizers(${target_name})

    find_package(GTest REQUIRED)

    target_link_libraries(${target_name} PRIVATE
        freecut::${TEST_NAME}
        GTest::gtest
        GTest::gtest_main
    )

    if(TEST_DEPENDS)
        target_link_libraries(${target_name} PRIVATE ${TEST_DEPENDS})
    endif()

    if(TEST_PRIVATE_DEPENDS)
        target_link_libraries(${target_name} PRIVATE ${TEST_PRIVATE_DEPENDS})
    endif()

    include(GoogleTest)
    gtest_discover_tests(${target_name})
endfunction()
