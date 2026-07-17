# SPDX-License-Identifier: MIT
#
# Copyright (c) 2025 Dylan Hong
#
# This file is released under the MIT License.
# For conditions of distribution and use, see the LICENSE file.

#============================================================================
# Detect QEMU ARM64 availability for cross-compile testing
#============================================================================

set(SGL_QEMU_AVAILABLE FALSE)
set(SGL_QEMU_EXECUTABLE "")
set(SGL_QEMU_RUNNER "")
set(SGL_QEMU_IS_CROSS_BUILD FALSE)

# Check whether this configure is producing Linux/AArch64 target binaries.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    if(NOT CMAKE_CROSSCOMPILING STREQUAL "FALSE")
        set(SGL_QEMU_IS_CROSS_BUILD TRUE)
    endif()
endif()

# Prefer qemu-aarch64-static because it is common on Linux/WSL CI images, then
# fall back to qemu-aarch64 for platforms that install only the dynamic runner.
if(SGL_QEMU_IS_CROSS_BUILD)
    find_program(QEMU_AARCH64_EXECUTABLE qemu-aarch64-static)

    if(QEMU_AARCH64_EXECUTABLE)
        set(SGL_QEMU_AVAILABLE TRUE)
        set(SGL_QEMU_EXECUTABLE ${QEMU_AARCH64_EXECUTABLE})
        message(STATUS "QEMU ARM64 found: ${SGL_QEMU_EXECUTABLE}")
    else()
        # Try without -static suffix
        find_program(QEMU_AARCH64_EXECUTABLE qemu-aarch64)

        if(QEMU_AARCH64_EXECUTABLE)
            set(SGL_QEMU_AVAILABLE TRUE)
            set(SGL_QEMU_EXECUTABLE ${QEMU_AARCH64_EXECUTABLE})
            message(STATUS "QEMU ARM64 found (non-static): ${SGL_QEMU_EXECUTABLE}")
        else()
            message(WARNING "Cross-compiling for ARM64 but QEMU ARM64 not found. Install qemu-user-static package.")
        endif()
    endif()
endif()

if(SGL_QEMU_AVAILABLE)
    # CTest and run-test.sh need the same sysroot that the toolchain used to link.
    set(SGL_QEMU_RUNNER ${SGL_QEMU_EXECUTABLE})
    if(CMAKE_SYSROOT)
        list(APPEND SGL_QEMU_RUNNER -L ${CMAKE_SYSROOT})
    endif()
endif()

# Register a test command that runs through QEMU when cross-built for AArch64.
function(add_qemu_test test_name test_executable)
    if(SGL_QEMU_AVAILABLE)
        add_test(
            NAME ${test_name}
            COMMAND ${SGL_QEMU_RUNNER} ${test_executable} ${ARGN}
        )
    else()
        message(WARNING "Skipping test '${test_name}': QEMU ARM64 not available")
    endif()
endfunction()
