# SPDX-License-Identifier: MIT
#
# Copyright (c) 2025 Dylan Hong
#
# This file is released under the MIT License.
# For conditions of distribution and use, see the LICENSE file.

include(${CMAKE_SOURCE_DIR}/script/config/test-dependencies.cmake)

# Keep direct configuration of the test subtree usable, even though the root
# project also enables testing for normal builds.
enable_testing()

include_directories(util)

# Build user-facing sample/benchmark/regression applications.
add_subdirectory(sample)
add_subdirectory(resize)
add_subdirectory(queue)
add_subdirectory(memory)

# Build the shared PNG helper library after dependency targets are declared.
add_subdirectory(util)
