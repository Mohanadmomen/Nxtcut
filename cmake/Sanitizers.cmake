# cmake/Sanitizers.cmake
# FreeCut-CPP sanitizer configuration.

option(FREECUT_ENABLE_ASAN "Enable AddressSanitizer (ASan)" OFF)
option(FREECUT_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer (UBSan)" OFF)
option(FREECUT_ENABLE_TSAN "Enable ThreadSanitizer (TSan)" OFF)

if(FREECUT_ENABLE_TSAN AND (FREECUT_ENABLE_ASAN OR FREECUT_ENABLE_UBSAN))
    message(FATAL_ERROR "ThreadSanitizer (TSAN) cannot be combined with AddressSanitizer (ASAN) or UndefinedBehaviorSanitizer (UBSAN).")
endif()

function(freecut_apply_sanitizers target_name)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(sanitizers "")

        if(FREECUT_ENABLE_ASAN)
            list(APPEND sanitizers "address")
        endif()

        if(FREECUT_ENABLE_UBSAN)
            list(APPEND sanitizers "undefined")
        endif()

        if(FREECUT_ENABLE_TSAN)
            list(APPEND sanitizers "thread")
        endif()

        if(sanitizers)
            string(JOIN "," sanitizer_list ${sanitizers})
            target_compile_options(${target_name} PRIVATE
                -fsanitize=${sanitizer_list}
                -fno-omit-frame-pointer
                -g
            )
            target_link_options(${target_name} PRIVATE
                -fsanitize=${sanitizer_list}
            )
        endif()

    elseif(MSVC)
        if(FREECUT_ENABLE_ASAN)
            target_compile_options(${target_name} PRIVATE /fsanitize=address)
        endif()

        if(FREECUT_ENABLE_UBSAN)
            message(STATUS "UndefinedBehaviorSanitizer (UBSan) is not supported by MSVC. Sanitizer is a no-op.")
        endif()

        if(FREECUT_ENABLE_TSAN)
            message(STATUS "ThreadSanitizer (TSan) is not supported by MSVC. Sanitizer is a no-op.")
        endif()

    else()
        if(FREECUT_ENABLE_ASAN OR FREECUT_ENABLE_UBSAN OR FREECUT_ENABLE_TSAN)
            message(STATUS "Sanitizers are not supported for compiler ${CMAKE_CXX_COMPILER_ID}. Sanitizers are a no-op.")
        endif()
    endif()
endfunction()
