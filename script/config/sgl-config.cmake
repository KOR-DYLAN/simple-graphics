# SPDX-License-Identifier: MIT
#
# Copyright (c) 2025 Dylan Hong
#
# This file is released under the MIT License.
# For conditions of distribution and use, see the LICENSE file.

include(${CMAKE_SOURCE_DIR}/option.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/compiler-warnings.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cppcheck/cppcheck.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/package/load-version.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/package/detect-install-dirs.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/arch/detect-arch.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/simd/detect-simd.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/thread/detect-thread.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/option-summary.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/package/package.cmake)

# Print the configuration summary to the console
sgl_print_configuration_summary()

configure_file(
    ${CMAKE_CURRENT_LIST_DIR}/sgl-config.h.in
    ${CMAKE_BINARY_DIR}/sgl-config.h
)

include_directories(${CMAKE_BINARY_DIR})
