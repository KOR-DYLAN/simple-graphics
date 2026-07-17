# SPDX-License-Identifier: MIT
#
# Copyright (c) 2025 Dylan Hong
#
# This file is released under the MIT License.
# For conditions of distribution and use, see the LICENSE file.

# Load user-facing cache options before any detection or target setup.
include(${CMAKE_SOURCE_DIR}/option.cmake)

# Configure build hygiene, package metadata, platform feature detection, and test runners.
include(${CMAKE_CURRENT_LIST_DIR}/compiler-warnings.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cppcheck/cppcheck.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/package/load-version.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/package/detect-install-dirs.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/arch/detect-arch.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/simd/detect-simd.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/thread/detect-thread.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/qemu/detect-qemu.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/option-summary.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/package/package.cmake)

# Print the configuration summary to the console.
sgl_print_configuration_summary()

# Generate the public compile-time configuration header consumed by SGL sources.
configure_file(
    ${CMAKE_CURRENT_LIST_DIR}/sgl-config.h.in
    ${CMAKE_BINARY_DIR}/sgl-config.h
)

include_directories(${CMAKE_BINARY_DIR})
