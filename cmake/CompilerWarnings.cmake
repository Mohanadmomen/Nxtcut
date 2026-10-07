# cmake/CompilerWarnings.cmake
# NxtCut-CPP compiler warnings configuration.

function(nxtcut_apply_warnings target_name)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE
            /W4
            /WX
            /permissive-
            /utf-8
        )
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(${target_name} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wshadow
            -Werror
        )
    else()
        message(AUTHOR_WARNING "Compiler ${CMAKE_CXX_COMPILER_ID} is not explicitly configured for strict warnings.")
    endif()
endfunction()
