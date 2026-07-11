# SPDX-License-Identifier: MIT
#
# Copyright (c) 2025 Dylan Hong
#
# This file is released under the MIT License.
# For conditions of distribution and use, see the LICENSE file.

project(memory
    VERSION 1.0
    LANGUAGES C
)

add_executable(${PROJECT_NAME} main.c)
target_link_libraries(${PROJECT_NAME} PRIVATE sgl-core)
set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 11)
