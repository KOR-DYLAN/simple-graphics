# SPDX-License-Identifier: MIT
#
# Copyright (c) 2025 Dylan Hong
#
# This file is released under the MIT License.
# For conditions of distribution and use, see the LICENSE file.

project(threadpool
    VERSION 1.0
    LANGUAGES C
)

# Exercise serialized submissions, worker claims, and queue preservation.
add_executable(${PROJECT_NAME} main.c)
target_link_libraries(${PROJECT_NAME} PRIVATE sgl-core)
set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 11)

if(SGL_QEMU_AVAILABLE)
    add_test(
        NAME ${PROJECT_NAME}
        COMMAND ${SGL_QEMU_RUNNER} $<TARGET_FILE:${PROJECT_NAME}>
    )
else()
    add_test(
        NAME ${PROJECT_NAME}
        COMMAND $<TARGET_FILE:${PROJECT_NAME}>
    )
endif()
