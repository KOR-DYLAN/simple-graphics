# SPDX-License-Identifier: MIT
#
# Copyright (c) 2025 Dylan Hong
#
# This file is released under the MIT License.
# For conditions of distribution and use, see the LICENSE file.

project(test-util
    VERSION 1.0
    LANGUAGES C CXX
)

list(APPEND SRC util.c)

# Static helper library for test PNG load/save and common utility functions.
add_library(${PROJECT_NAME} ${SRC})

# Ensure zlib/libpng external projects are built before this helper is linked.
add_dependencies(${PROJECT_NAME} sgl-test-dependencies)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE SGLTest::PNG SGLTest::ZLIB)
set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 11)
