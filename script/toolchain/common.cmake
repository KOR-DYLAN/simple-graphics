# SPDX-License-Identifier: MIT
#
# Copyright (c) 2025 Dylan Hong
#
# This file is released under the MIT License.
# For conditions of distribution and use, see the LICENSE file.

# Shared helpers for the project toolchain files.
#
# CMake reads a toolchain file before project() enables languages and detects the
# compiler ABI.  Values set here become the initial compiler contract for the
# whole build tree, so the helpers below intentionally avoid target-level logic
# and only prepare compiler, sysroot, and search-path defaults.

# Set the common Debug/Release optimization flags before compiler checks run.
function(sgl_toolchain_set_standard_build_flags)
    # Keep the build-type flags identical for every supported toolchain.
    #
    # These are *_FLAGS_<CONFIG> variables rather than target_compile_options()
    # because they must be available as soon as CMake creates the initial
    # compiler checks.  The project can still add target-specific warnings or
    # defines later from normal CMakeLists.txt files.
    foreach(SGL_LANG ASM C CXX)
        set(CMAKE_${SGL_LANG}_FLAGS_DEBUG
            "-O0 -g" PARENT_SCOPE)
        set(CMAKE_${SGL_LANG}_FLAGS_MINSIZEREL
            "-Os -DNDEBUG" PARENT_SCOPE)
        set(CMAKE_${SGL_LANG}_FLAGS_RELEASE
            "-O2 -DNDEBUG" PARENT_SCOPE)
        set(CMAKE_${SGL_LANG}_FLAGS_RELWITHDEBINFO
            "-O2 -g -DNDEBUG" PARENT_SCOPE)
    endforeach()
endfunction()

# Detect and cache the target sysroot from the selected GCC-compatible compiler.
function(sgl_toolchain_detect_sysroot SGL_COMPILER)
    # Ask the GCC-compatible compiler where its C library and system headers
    # live.  For the aarch64 cross builds this is the sysroot used by both GCC
    # and Clang.  We also write the detected path for the Makefile run target,
    # which needs the same root when launching qemu-aarch64.
    execute_process(
        COMMAND ${SGL_COMPILER} -print-sysroot
        RESULT_VARIABLE SGL_SYSROOT_RESULT
        OUTPUT_VARIABLE SGL_DETECTED_SYSROOT
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if(SGL_SYSROOT_RESULT EQUAL 0 AND NOT "${SGL_DETECTED_SYSROOT}" STREQUAL "")
        set(CMAKE_SYSROOT "${SGL_DETECTED_SYSROOT}"
            CACHE PATH "Sysroot path" FORCE)
        set(CMAKE_SYSROOT "${SGL_DETECTED_SYSROOT}" PARENT_SCOPE)
        file(WRITE "${CMAKE_BINARY_DIR}/sysroot.txt"
            "${SGL_DETECTED_SYSROOT}\n")
        message(STATUS "Sysroot detected: ${SGL_DETECTED_SYSROOT}")
    else()
        message(WARNING
            "Failed to detect sysroot from ${SGL_COMPILER}")
    endif()
endfunction()

# Find the GCC toolchain root that Clang needs for cross runtime pieces.
function(sgl_toolchain_find_gcc_root SGL_OUTPUT SGL_GCC)
    # Clang can emit aarch64 code directly, but it still needs a GCC toolchain
    # root for libgcc, crt objects, linker scripts, and the cross libc layout.
    # The root is the parent directory of the cross gcc binary directory.
    execute_process(
        COMMAND which ${SGL_GCC}
        RESULT_VARIABLE SGL_GCC_RESULT
        OUTPUT_VARIABLE SGL_GCC_PATH
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if(SGL_GCC_RESULT EQUAL 0 AND NOT "${SGL_GCC_PATH}" STREQUAL "")
        get_filename_component(SGL_GCC_BIN "${SGL_GCC_PATH}" DIRECTORY)
        get_filename_component(SGL_GCC_ROOT "${SGL_GCC_BIN}" DIRECTORY)
        set(${SGL_OUTPUT} "${SGL_GCC_ROOT}" PARENT_SCOPE)
    else()
        message(WARNING
            "Failed to find GCC toolchain root from ${SGL_GCC}")
        set(${SGL_OUTPUT} "" PARENT_SCOPE)
    endif()
endfunction()

# Configure CMake find roots so cross builds prefer target headers/libraries.
function(sgl_toolchain_append_project_find_roots SGL_EXTRA_ROOT SGL_PROGRAM_MODE)
    # Cross builds must prefer target libraries and headers over host files.
    # Some toolchains also need their sysroot in the CMake find root list; pass
    # it as SGL_EXTRA_ROOT when that is required.
    set(SGL_FIND_ROOTS)

    if(NOT "${SGL_EXTRA_ROOT}" STREQUAL "")
        list(APPEND SGL_FIND_ROOTS "${SGL_EXTRA_ROOT}")
    endif()

    list(APPEND CMAKE_FIND_ROOT_PATH ${SGL_FIND_ROOTS})
    set(CMAKE_FIND_ROOT_PATH "${CMAKE_FIND_ROOT_PATH}" PARENT_SCOPE)

    if(NOT "${SGL_PROGRAM_MODE}" STREQUAL "")
        set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM
            "${SGL_PROGRAM_MODE}" PARENT_SCOPE)
    endif()

    set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY PARENT_SCOPE)
    set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY PARENT_SCOPE)
endfunction()
