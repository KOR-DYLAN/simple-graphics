# SPDX-License-Identifier: MIT
#
# Copyright (c) 2025 Dylan Hong
#
# This file is released under the MIT License.
# For conditions of distribution and use, see the LICENSE file.

include(${CMAKE_SOURCE_DIR}/script/config/test-dependencies.cmake)

include_directories(util)
add_subdirectory(sample)
add_subdirectory(resize)
add_subdirectory(queue)
add_subdirectory(memory)
add_subdirectory(util)
