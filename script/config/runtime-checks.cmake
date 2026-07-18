# SPDX-License-Identifier: MIT
#
# Copyright (c) 2025 Dylan Hong
#
# This file is released under the MIT License.
# For conditions of distribution and use, see the LICENSE file.

set(SGL_SANITIZER_STATUS "OFF")
set(SGL_STACK_PROTECTOR_STATUS "OFF")

# Runtime checks are directory-scope build instrumentation.  This file is
# included before project targets are declared, so add_compile_options() and
# add_link_options() cover the SGL library and test executables consistently.
function(sgl_add_compile_option_for_c_family OPTION)
    add_compile_options(
        "$<$<COMPILE_LANGUAGE:C>:${OPTION}>"
        "$<$<COMPILE_LANGUAGE:CXX>:${OPTION}>"
    )
endfunction()

function(sgl_enable_sanitizers)
    if(NOT WITH_SANITIZER)
        return()
    endif()

    if(NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR
            "WITH_SANITIZER is supported only for GNU/Clang-compatible compilers")
    endif()

    # Accept both Make/CMake command-line styles:
    #   SANITIZERS=address,undefined
    #   -DSANITIZERS=address\;undefined
    string(REPLACE " " "" SGL_SANITIZER_LIST "${SANITIZERS}")
    string(REPLACE ";" "," SGL_SANITIZER_LIST "${SGL_SANITIZER_LIST}")

    if("${SGL_SANITIZER_LIST}" STREQUAL "")
        message(FATAL_ERROR
            "WITH_SANITIZER is ON, but SANITIZERS is empty")
    endif()

    # TSan replaces the allocator/threading runtime hooks used by ASan/LSan.
    # Keep it as a separate build so the selected sanitizer runtime is coherent.
    if((SGL_SANITIZER_LIST MATCHES "(^|,)thread(,|$)") AND
       (SGL_SANITIZER_LIST MATCHES "(^|,)(address|leak)(,|$)"))
        message(FATAL_ERROR
            "thread sanitizer cannot be combined with address or leak sanitizer")
    endif()

    if((SGL_SANITIZER_LIST MATCHES "(^|,)memory(,|$)") AND
       (NOT CMAKE_C_COMPILER_ID MATCHES "Clang"))
        message(FATAL_ERROR
            "memory sanitizer is supported only with Clang")
    endif()

    set(SGL_SANITIZER_FLAG "-fsanitize=${SGL_SANITIZER_LIST}")
    sgl_add_compile_option_for_c_family("${SGL_SANITIZER_FLAG}")
    # Frame pointers make sanitizer reports actionable in optimized and mixed
    # C/C++ test binaries.
    sgl_add_compile_option_for_c_family("-fno-omit-frame-pointer")
    add_link_options("${SGL_SANITIZER_FLAG}")

    set(SGL_SANITIZER_STATUS
        "ON (${SGL_SANITIZER_LIST}; -fno-omit-frame-pointer)"
        PARENT_SCOPE)
    message(STATUS "Sanitizers: ${SGL_SANITIZER_LIST}")
endfunction()

function(sgl_enable_stack_protector)
    if(NOT WITH_STACK_PROTECTOR)
        return()
    endif()

    if(NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR
            "WITH_STACK_PROTECTOR is supported only for GNU/Clang-compatible compilers")
    endif()

    # Expose a small stable vocabulary instead of leaking raw compiler flags
    # into common Makefile/CMake invocations.
    string(TOLOWER "${STACK_PROTECTOR_MODE}" SGL_STACK_PROTECTOR_MODE)
    if("${SGL_STACK_PROTECTOR_MODE}" STREQUAL "basic")
        set(SGL_STACK_PROTECTOR_FLAG "-fstack-protector")
    elseif("${SGL_STACK_PROTECTOR_MODE}" STREQUAL "strong")
        set(SGL_STACK_PROTECTOR_FLAG "-fstack-protector-strong")
    elseif("${SGL_STACK_PROTECTOR_MODE}" STREQUAL "all")
        set(SGL_STACK_PROTECTOR_FLAG "-fstack-protector-all")
    else()
        message(FATAL_ERROR
            "Unsupported STACK_PROTECTOR_MODE: ${STACK_PROTECTOR_MODE}. "
            "Use basic, strong, or all.")
    endif()

    sgl_add_compile_option_for_c_family("${SGL_STACK_PROTECTOR_FLAG}")
    add_link_options("${SGL_STACK_PROTECTOR_FLAG}")

    set(SGL_STACK_PROTECTOR_STATUS
        "ON (${SGL_STACK_PROTECTOR_FLAG})"
        PARENT_SCOPE)
    message(STATUS "Stack protector: ${SGL_STACK_PROTECTOR_FLAG}")
endfunction()

sgl_enable_sanitizers()
sgl_enable_stack_protector()
